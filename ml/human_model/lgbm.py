"""LightGBM move model (#85), compared with the logit and the network in Python.

  python -m ml.human_model.lgbm [--features data/features] [--store data/turns]
                                [--top-k 256] [--max-train 60000] [--max-val 20000]
                                [--movers human] [--trees 300] [--leaves 31]
                                [--report lgbm.json]

A LambdaRank model (lightgbm.LGBMRanker, the optional "lgbm" extra) scores each
turn's top-k candidates from the same features as the logit, trained with the
played candidate as the only relevant one. Its scores become probabilities by a
softmax with a temperature fitted on val, then the engine's mixture (epsilon,
residual) is tuned on val as for the logit, so its val and test NLL are the
same quantity as ml.human_model.logit's. The engine keeps loading the logit or
network file; this model is only compared, and exported later if it wins.
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import numpy as np

from ml.data.features import DEFAULT_OUT as DEFAULT_FEATURES
from ml.data.features import header, load, ratings
from ml.data.store import DEFAULT_STORE
from ml.human_model.dataset import TurnSet, log_softmax_chosen, mixture_nll, select, tune_mixture


def ranking_data(ts: TurnSet) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """(rows, labels, group sizes) of the turns whose played option is a candidate."""
    idx = np.flatnonzero(ts.inside)
    x, starts = ts.gather(idx)
    y = np.zeros(len(x))
    y[starts + ts.chosen[idx]] = 1.0
    return x, y, ts.k[idx]


def scores(model, ts: TurnSet) -> tuple[np.ndarray, np.ndarray]:
    """(stacked scores, starts) for every turn of ts."""
    parts, starts, at = [], [], 0
    for chunk in ts.chunks():
        x, s = ts.gather(chunk)
        parts.append(model.predict(x))
        starts.append(s + at)
        at += len(x)
    if not parts:
        return np.zeros(0), np.zeros(0, dtype=np.int64)
    return np.concatenate(parts), np.concatenate(starts)


def softmax_log_p(u: np.ndarray, starts: np.ndarray, ts: TurnSet, temperature: float) -> np.ndarray:
    """log P(played candidate) per turn (0 for turns outside), scores divided by T."""
    if len(ts) == 0:
        return np.zeros(0)
    return np.where(ts.inside, log_softmax_chosen(u / temperature, starts, ts.chosen), 0.0)


def fit_temperature(u: np.ndarray, starts: np.ndarray, ts: TurnSet, lo=0.05, hi=20.0) -> float:
    """The temperature minimizing the softmax NLL of the inside turns (golden section on log T)."""
    inside = ts.inside

    def nll(log_t: float) -> float:
        return float(-softmax_log_p(u, starts, ts, float(np.exp(log_t)))[inside].mean())

    a, b = np.log(lo), np.log(hi)
    g = (np.sqrt(5) - 1) / 2
    c, d = b - g * (b - a), a + g * (b - a)
    for _ in range(60):
        if nll(c) < nll(d):
            b = d
        else:
            a = c
        c, d = b - g * (b - a), a + g * (b - a)
    return float(np.exp((a + b) / 2))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--features", type=Path, default=DEFAULT_FEATURES)
    ap.add_argument("--store", type=Path, default=DEFAULT_STORE)
    ap.add_argument("--top-k", type=int, default=256)
    ap.add_argument("--max-train", type=int, default=60000)
    ap.add_argument("--max-val", type=int, default=20000)
    ap.add_argument("--movers", default="human", choices=("human", "bot", "all"))
    ap.add_argument("--trees", type=int, default=300)
    ap.add_argument("--leaves", type=int, default=31)
    ap.add_argument("--learning-rate", type=float, default=0.05)
    ap.add_argument("--threads", type=int, default=2)
    ap.add_argument("--report", type=Path)
    args = ap.parse_args()
    try:
        import lightgbm
    except ImportError:
        raise SystemExit("lightgbm is missing: pip install -e '.[lgbm]'") from None
    start = time.time()
    names, _, _ = header(args.features / "options.bin")
    turns, options = load(args.features)
    info = ratings(args.features, args.store)
    bands = (0,)
    k = args.top_k
    train = select(turns, options, info, "train", k, bands, args.movers, max_turns=args.max_train)
    val = select(turns, options, info, "val", k, bands, args.movers, max_turns=args.max_val, seed=1)
    test = select(turns, options, info, "test", k, bands, args.movers)
    x, y, groups = ranking_data(train)
    model = lightgbm.LGBMRanker(
        objective="lambdarank",
        n_estimators=args.trees,
        num_leaves=args.leaves,
        learning_rate=args.learning_rate,
        n_jobs=args.threads,
        verbose=-1,
    )
    model.fit(x, y, group=groups)
    u_val, s_val = scores(model, val)
    temperature = fit_temperature(u_val, s_val, val)
    lp_val = softmax_log_p(u_val, s_val, val, temperature)
    epsilon, residual, val_nll = tune_mixture(lp_val, val)
    u_test, s_test = scores(model, test)
    test_nll = mixture_nll(
        softmax_log_p(u_test, s_test, test, temperature), test, epsilon, residual
    )
    report = {
        "features": len(names),
        "train_turns": len(train),
        "val_turns": len(val),
        "test_turns": len(test),
        "top_k": k,
        "trees": args.trees,
        "leaves": args.leaves,
        "temperature": temperature,
        "epsilon": epsilon,
        "residual": residual,
        "val_nll_lgbm": val_nll,
        "test_nll_lgbm": test_nll,
        "seconds": round(time.time() - start, 1),
    }
    print(json.dumps(report))
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=1) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
