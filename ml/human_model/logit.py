"""Conditional logit model of human move choice, shrunk by rating band.

P(option i | turn) = exp(w . x_i) / sum_j exp(w . x_j) over the turn's top-k
options by static equity (the rows written by ml.data.features), x the
standardized features. A common model is fitted with an elastic-net penalty;
each rating band's weights are then shrunk towards it. The penalties, the
mixture's epsilon and residual and top_k are chosen on the val split. With
--mlp a small network (ml.human_model.mlp) is also fitted and kept if it beats
the logit on val; the logit weights stay in the file as the fallback.

  python -m ml.human_model.logit [--features data/features] [--store data/turns]
                                 [--out models/human_model.txt] [--bands 0,1400,1700,2000]
                                 [--top-k 64,96,128] [--drop A,B] [--mlp] [--test]
                                 [--report report.json] [--min-rating R] [--max-rating R]

The output is read by HumanModelParams::FromFile in the engine.
"""

from __future__ import annotations

import argparse
import json
import time
from collections.abc import Sequence
from pathlib import Path

import numpy as np

from ml.data.features import DEFAULT_OUT as DEFAULT_FEATURES
from ml.data.features import header, load, ratings
from ml.data.store import DEFAULT_STORE
from ml.human_model.dataset import (
    Scaler,
    TurnSet,
    band_of,
    banded_mixture_nll,
    columns,
    log_softmax_chosen,
    select,
    tune_band_mixtures,
    tune_mixture,
)

DEFAULT_MODEL = Path(__file__).resolve().parents[2] / "models" / "human_model.txt"
DEFAULT_BANDS = (0, 1400, 1700, 2000)
__all__ = ["band_of", "fit", "fit_bands", "in_rating_range", "log_p", "write_params"]


def _objective(ts: TurnSet, scaler: Scaler, w: np.ndarray, hessian: bool):
    """(log-likelihood, gradient, Hessian or None) summed over the inside turns of ts."""
    ll, grad = 0.0, np.zeros_like(w)
    hess = np.zeros((len(w), len(w))) if hessian else None
    for chunk in ts.chunks(np.flatnonzero(ts.inside)):
        x, starts = ts.gather(chunk)
        x = scaler(x)
        u = x @ w
        ids = np.repeat(np.arange(len(starts)), ts.k[chunk])
        m = np.maximum.reduceat(u, starts)
        e = np.exp(u - m[ids])
        z = np.add.reduceat(e, starts)
        p = e / z[ids]
        chosen = starts + ts.chosen[chunk]
        ll += float(u[chosen].sum() - (np.log(z) + m).sum())
        px = p[:, None] * x
        mean = np.add.reduceat(px, starts)
        grad += x[chosen].sum(0) - mean.sum(0)
        if hessian:
            hess += px.T @ x - mean.T @ mean
    return ll, grad, hess


def fit(
    ts: TurnSet,
    scaler: Scaler,
    l2: float = 1e-3,
    l1: float = 0.0,
    prior=None,
    shrink: float = 0.0,
    w0=None,
    iters: int = 30,
    tol: float = 1e-6,
):
    """Penalized maximum-likelihood weights (standardized features) by damped Newton.

    Minimizes -LL / N + l1 |w|_1 + l2 / 2 |w|^2 + shrink / 2 |w - prior|^2 over
    the N inside turns; the L1 term by a local quadratic approximation.
    """
    d = len(ts.cols)
    n = max(int(ts.inside.sum()), 1)
    w = np.zeros(d) if w0 is None else np.array(w0, dtype=float)
    prior = np.zeros(d) if prior is None else np.asarray(prior)

    def penalty(v):
        return l1 * np.abs(v).sum() + 0.5 * l2 * v @ v + 0.5 * shrink * (v - prior) @ (v - prior)

    ll, g, h = _objective(ts, scaler, w, True)
    obj = -ll / n + penalty(w)
    for _ in range(iters):
        lqa = l1 / (np.abs(w) + 1e-4)
        grad = -g / n + l2 * w + shrink * (w - prior) + lqa * w
        hess = h / n + np.diag(l2 + shrink + lqa + 1e-9)
        step = -np.linalg.solve(hess, grad)
        # Backtracking: a full Newton step can overshoot far from the optimum.
        for _ in range(30):
            new = w + step
            new_ll, new_g, new_h = _objective(ts, scaler, new, True)
            new_obj = -new_ll / n + penalty(new)
            if new_obj <= obj + 1e-12:
                break
            step /= 2
        else:
            break
        w, obj, g, h = new, new_obj, new_g, new_h
        if np.abs(step).max() < tol:
            break
    if l1 > 0:
        w[np.abs(w) < 1e-3] = 0.0
    return w


def log_p(ts: TurnSet, scaler: Scaler, weights) -> np.ndarray:
    """Softmax log probability of the played candidate per turn (0 for turns outside).

    weights: one vector, or one per band (indexed by ts.band)."""
    out = np.zeros(len(ts))
    per_band = np.ndim(weights) == 2
    for chunk in ts.chunks(np.flatnonzero(ts.inside)):
        x, starts = ts.gather(chunk)
        x = scaler(x)
        if per_band:
            band = np.repeat(ts.band[chunk], ts.k[chunk])
            u = np.einsum("ij,ij->i", x, np.asarray(weights)[band])
        else:
            u = x @ weights
        out[chunk] = log_softmax_chosen(u, starts, ts.chosen[chunk])
    return out


def fit_bands(
    train: TurnSet,
    val: TurnSet,
    scaler: Scaler,
    common,
    n_bands: int,
    shrinks=(3.0, 1.0, 0.3, 0.1, 0.03, 0.01),
):
    """(weights per band, shrink): band weights shrunk towards `common` with the
    strength (per turn) that gives the lowest val softmax NLL; a band with no turns
    keeps the common weights."""
    best = None
    for s in shrinks:
        weights = []
        for b in range(n_bands):
            sub = train.subset(np.flatnonzero(train.band == b))
            weights.append(
                fit(sub, scaler, l2=0.0, prior=common, shrink=s, w0=common)
                if sub.inside.any()
                else np.array(common)
            )
        weights = np.array(weights)
        nll = -log_p(val, scaler, weights)[val.inside].mean()
        if best is None or nll < best[2]:
            best = (weights, s, nll)
    return best[0], best[1]


def in_rating_range(info: dict, low: int | None, high: int | None) -> dict:
    """info without the turns whose known rating lies outside [low, high]; turns
    without a rating (e.g. self-play) stay."""
    return {
        key: v
        for key, v in info.items()
        if v[0] is None or ((low is None or v[0] >= low) and (high is None or v[0] <= high))
    }


def raw_weights(w: np.ndarray, scaler: Scaler) -> np.ndarray:
    """Weights on the unstandardized features (the mean shift cancels in a softmax)."""
    return w / scaler.scale


def write_params(
    path: Path,
    names: Sequence[str],
    weights,
    bands: Sequence[int],
    top_k: int,
    epsilon: float,
    residual: float,
    mlp_lines: Sequence[str] = (),
    mixtures=None,
) -> None:
    """weights: one raw-scale vector per band, in the order of `names`; mixtures: optional
    (epsilon, residual) per band."""
    lines = [
        "# human move model: conditional logit by rating band"
        + (", and a network" if mlp_lines else ""),
        "version 2",
        f"kind {'mlp' if mlp_lines else 'logit'}",
        "features " + " ".join(names),
        f"top_k {top_k}",
        f"epsilon {epsilon:g}",
        f"residual {residual:.6g}",
    ]
    for low, w in zip(bands, weights, strict=True):
        lines.append(f"band {low} " + " ".join(f"{x:.6g}" for x in w))
    for low, (e, r) in zip(bands, mixtures or (), strict=False):
        lines.append(f"mixture {low} {e:g} {max(r, 1e-4):.6g}")
    lines += list(mlp_lines)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--features", type=Path, default=DEFAULT_FEATURES)
    ap.add_argument("--store", type=Path, default=DEFAULT_STORE)
    ap.add_argument("--out", type=Path, default=DEFAULT_MODEL)
    ap.add_argument("--bands", default=",".join(map(str, DEFAULT_BANDS)))
    ap.add_argument("--split", default="train")
    ap.add_argument("--val-split", default="val")
    ap.add_argument("--top-k", default="128,192,256", help="top_k values to try on val")
    ap.add_argument("--l2", default="1e-3,1e-2,1e-1", help="L2 strengths to try")
    ap.add_argument("--l1", default="0,1e-3,1e-2", help="L1 strengths to try")
    ap.add_argument(
        "--movers",
        default="human",
        choices=("human", "bot", "all"),
        help="whose turns to fit: humans (the human model) or Woogles' bots",
    )
    ap.add_argument("--drop", default="", help="feature groups to leave out, e.g. A,B")
    ap.add_argument("--min-rating", type=int, help="leave out turns rated below this")
    ap.add_argument("--max-rating", type=int, help="leave out turns rated above this")
    ap.add_argument("--max-train", type=int, default=60000)
    ap.add_argument("--max-val", type=int, default=20000)
    ap.add_argument(
        "--search-train",
        type=int,
        default=20000,
        help="train turns used to choose the penalties (0: all)",
    )
    ap.add_argument("--mlp-max-train", type=int, default=150000)
    ap.add_argument("--mlp-hidden", type=int, default=32)
    ap.add_argument("--mlp-lr", type=float, default=5e-4)
    ap.add_argument("--mlp-decay", type=float, default=1e-3)
    ap.add_argument("--mlp", action="store_true", help="also fit a network, keep it if better")
    ap.add_argument("--test", action="store_true", help="also report the test split")
    ap.add_argument("--report", type=Path)
    ap.add_argument("--progress", action="store_true")
    args = ap.parse_args()
    start = time.time()
    bands = [int(b) for b in args.bands.split(",")]
    ks = sorted(int(k) for k in args.top_k.split(","))
    names, _, _ = header(args.features / "options.bin")
    cols = columns(names, [g for g in args.drop.split(",") if g])
    kept = [names[i] for i in cols]
    turns, options = load(args.features)
    info = in_rating_range(ratings(args.features, args.store), args.min_rating, args.max_rating)
    steps = 2 + len(args.l2.split(",")) * len(args.l1.split(",")) + 2 * bool(args.mlp)
    done = 0

    def tick():
        nonlocal done
        done += 1
        if args.progress:
            print(f"progress: {done}/{steps}", flush=True)

    train = select(
        turns, options, info, args.split, ks[-1], bands, args.movers, max_turns=args.max_train
    ).with_cols(cols)
    val = select(
        turns,
        options,
        info,
        args.val_split,
        ks[-1],
        bands,
        args.movers,
        max_turns=args.max_val,
        seed=1,
    ).with_cols(cols)
    if not len(train) or not len(val):
        raise SystemExit(f"no turns: {len(train)} {args.split}, {len(val)} {args.val_split}")
    report = {
        "movers": args.movers,
        "features": len(kept),
        "dropped": args.drop,
        "train_turns": len(train),
        "val_turns": len(val),
        "coverage": {k: val.with_top(k).coverage() for k in ks},
    }
    scaler = Scaler.fit(train)

    # Penalties chosen on a subset of train (cheaper), then the final fit on all of
    # it with the penalties scaled to its size (per-turn penalties shrink as 1/N).
    search = train
    if 0 < args.search_train < len(train):
        search = train.subset(
            np.sort(np.random.default_rng(2).choice(len(train), args.search_train, replace=False))
        )
    best = None
    w = None
    for l2 in (float(x) for x in args.l2.split(",")):
        for l1 in (float(x) for x in args.l1.split(",")):
            w = fit(search, scaler, l2=l2, l1=l1, w0=w)
            nll = -log_p(val, scaler, w)[val.inside].mean()
            if best is None or nll < best[0]:
                best = (nll, l2, l1, w)
            tick()
    _, l2, l1, common = best
    _, shrink = fit_bands(search, val, scaler, common, len(bands))
    scale = len(search) / len(train)
    if search is not train:
        common = fit(train, scaler, l2=l2 * scale, l1=l1 * scale, w0=common)
    weights, _ = fit_bands(train, val, scaler, common, len(bands), shrinks=(shrink * scale,))
    report.update(
        l2=l2 * scale,
        l1=l1 * scale,
        nonzero=int((common != 0).sum()),
        shrink=shrink * scale,
        search_turns=len(search),
    )
    # Values chosen at the edge of their grid: widen it.
    report["at_grid_edge"] = [
        name
        for name, v, grid in (("l2", l2, args.l2), ("l1", l1, args.l1))
        if v == max(float(x) for x in grid.split(","))
    ]
    tick()

    # Mixture and top_k on val.
    tuned = None
    for k in ks:
        v = val.with_top(k)
        e, r, nll = tune_mixture(log_p(v, scaler, weights), v)
        if tuned is None or nll < tuned[3]:
            tuned = (k, e, r, nll)
    top_k, epsilon, residual, logit_nll = tuned
    v = val.with_top(top_k)
    logit_lp = log_p(v, scaler, weights)
    mixtures, banded = tune_band_mixtures(logit_lp, v, len(bands), (epsilon, residual))
    report.update(
        top_k=top_k,
        epsilon=epsilon,
        residual=residual,
        val_nll_logit_global=logit_nll,
        mixtures=mixtures,
    )
    logit_nll = banded
    report["val_nll_logit"] = logit_nll
    tick()

    raw = [raw_weights(wb, scaler) for wb in weights]
    mlp_lines, kind = (), "logit"
    if args.mlp:
        from ml.human_model import mlp

        net_train = select(
            turns,
            options,
            info,
            args.split,
            top_k,
            bands,
            args.movers,
            max_turns=args.mlp_max_train,
        ).with_cols(cols)
        net = mlp.fit(
            net_train,
            val.with_top(top_k),
            scaler,
            hidden=args.mlp_hidden,
            lr=args.mlp_lr,
            decay=args.mlp_decay,
            skip=common,
        )
        tick()
        v = val.with_top(top_k)
        net_lp = mlp.log_p(net, v, scaler)
        net_e, net_r, _ = tune_mixture(net_lp, v)
        net_mix, net_nll = tune_band_mixtures(net_lp, v, len(bands), (net_e, net_r))
        report.update(
            val_nll_mlp=net_nll,
            mlp_epsilon=net_e,
            mlp_residual=net_r,
            mlp_mixtures=net_mix,
            mlp_temperature=net.temperature,
            mlp_hidden=net.hidden,
            mlp_epochs=net.epochs,
        )
        if net_nll < logit_nll:
            mlp_lines, kind = mlp.param_lines(net, scaler), "mlp"
            epsilon, residual, mixtures = net_e, net_r, net_mix
        tick()
    report["kind"] = kind
    write_params(
        args.out, kept, raw, bands, top_k, epsilon, max(residual, 1e-4), mlp_lines, mixtures
    )

    if args.test:
        test = select(turns, options, info, "test", top_k, bands, args.movers).with_cols(cols)
        report["test_turns"] = len(test)
        report["test_coverage"] = test.coverage()
        report["test_nll_logit"] = banded_mixture_nll(
            log_p(test, scaler, weights), test, report["mixtures"]
        )
        if args.mlp:
            report["test_nll_mlp"] = banded_mixture_nll(
                mlp.log_p(net, test, scaler), test, report["mlp_mixtures"]
            )
    report["seconds"] = round(time.time() - start, 1)
    print(json.dumps(report))
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=1) + "\n")
    print(
        f"{kind} model on {len(kept)} features, val NLL {report['val_nll_logit']:.4f} (logit)"
        + (f" {report['val_nll_mlp']:.4f} (mlp)" if args.mlp else "")
        + f", top_k {top_k}, written to {args.out}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
