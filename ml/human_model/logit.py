"""Conditional logit model of human move choice, one set of weights per rating band.

P(option i | turn) = exp(w . x_i) / sum_j exp(w . x_j) over the turn's top options
by static equity (the rows written by ml.data.features). Turns whose played
option ranks below the top give the residual mass the engine spreads over the
other options.

  python -m ml.human_model.logit [--features data/features] [--store data/turns]
                                 [--out models/human_model.txt] [--bands 0,1400,1700,2000]

The output is read by HumanModelParams::FromFile in the engine.
"""

from __future__ import annotations

import argparse
from collections.abc import Sequence
from pathlib import Path

from ml.data.features import DEFAULT_OUT as DEFAULT_FEATURES
from ml.data.features import FEATURES, load, ratings
from ml.data.store import DEFAULT_STORE

DEFAULT_MODEL = Path(__file__).resolve().parents[2] / "models" / "human_model.txt"
DEFAULT_BANDS = (0, 1400, 1700, 2000)


def fit(groups, l2: float = 1e-3, iters: int = 50, tol: float = 1e-8):
    """Maximum-likelihood weights by Newton's method.

    groups: (X, y) pairs, X the (options, features) rows of a turn and y the
    row of the chosen option. A small L2 penalty keeps constant features finite.
    """
    import numpy as np

    groups = [(np.asarray(x, dtype=float), y) for x, y in groups if len(x) > 0 and y >= 0]
    if not groups:
        return np.zeros(len(FEATURES))
    x = np.concatenate([g[0] for g in groups])
    starts = np.cumsum([0] + [len(g[0]) for g in groups[:-1]])
    chosen = x[starts + np.array([g[1] for g in groups])]
    ids = np.repeat(np.arange(len(groups)), [len(g[0]) for g in groups])
    w = np.zeros(x.shape[1])
    for _ in range(iters):
        u = x @ w
        u -= np.maximum.reduceat(u, starts)[ids]
        e = np.exp(u)
        p = e / np.add.reduceat(e, starts)[ids]
        mean = np.add.reduceat(p[:, None] * x, starts)  # expected features per turn
        grad = chosen.sum(0) - mean.sum(0) - l2 * w
        hess = (p[:, None] * x).T @ x - mean.T @ mean + l2 * np.eye(len(w))
        step = np.linalg.solve(hess, grad)
        w += step
        if np.abs(step).max() < tol:
            break
    return w


def band_of(rating: int | None, bands: Sequence[int]) -> int:
    """Index of the band a rating falls in; unknown ratings go to the first."""
    band = 0
    for i, low in enumerate(bands):
        if rating is not None and rating >= low:
            band = i
    return band


def fit_bands(turns, options, rating_of, bands: Sequence[int] = DEFAULT_BANDS, l2: float = 1e-3):
    """(weights per band, residual): rating_of maps a turn to its rating or None.

    The residual is the share of turns (with an option we generate) whose played
    option ranks below the top rows.
    """
    groups = [[] for _ in bands]
    outside = inside = 0
    for t in turns:
        if t.chosen < 0:
            continue
        if t.chosen >= t.top:
            outside += 1
            continue
        inside += 1
        rows = options[t.first_row : t.first_row + t.top]
        groups[band_of(rating_of(t), bands)].append((rows, t.chosen))
    weights = [fit(g, l2) for g in groups]
    residual = outside / (inside + outside) if inside + outside else 0.0
    return weights, residual


def write_params(
    path: Path, weights, bands: Sequence[int], top_k: int, epsilon: float, residual: float
) -> None:
    lines = [
        "# conditional logit by rating band; features: " + " ".join(FEATURES),
        f"top_k {top_k}",
        f"epsilon {epsilon:g}",
        f"residual {residual:.6g}",
    ]
    for low, w in zip(bands, weights, strict=True):
        lines.append(f"band {low} " + " ".join(f"{x:.6g}" for x in w))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--features", type=Path, default=DEFAULT_FEATURES)
    ap.add_argument("--store", type=Path, default=DEFAULT_STORE)
    ap.add_argument("--out", type=Path, default=DEFAULT_MODEL)
    ap.add_argument("--bands", default=",".join(map(str, DEFAULT_BANDS)))
    ap.add_argument("--split", default="train")
    ap.add_argument("--epsilon", type=float, default=0.05)
    ap.add_argument("--l2", type=float, default=1e-3)
    args = ap.parse_args()
    bands = [int(b) for b in args.bands.split(",")]
    turns, options = load(args.features)
    info = ratings(args.features, args.store)
    turns = [t for t in turns if info.get((t.game_id, t.turn), (None, ""))[1] == args.split]
    top_k = max((t.top for t in turns), default=1)
    weights, residual = fit_bands(
        turns, options, lambda t: info[(t.game_id, t.turn)][0], bands, args.l2
    )
    write_params(args.out, weights, bands, top_k, args.epsilon, max(residual, 1e-4))
    print(f"{len(turns)} turns, residual {residual:.4f}, written to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
