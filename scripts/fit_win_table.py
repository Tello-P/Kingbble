#!/usr/bin/env python3
"""Fits the win table from the counts of kingbble_winstats.

Sums every winstats file under the given folders. For each tile count it smooths
the win rate over nearby spreads and makes it grow with the spread; a tile count
with too few positions takes the nearest one with enough.

  python scripts/fit_win_table.py resultados-todos --out configs/win_table_csw24.csv
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path

MAX_SPREAD = 300
STEP = 5
MAX_UNSEEN = 93
SPREADS = list(range(-MAX_SPREAD, MAX_SPREAD + 1, STEP))
MIN_POSITIONS = 2000
WIDTH = 2  # table steps on each side of a spread that it borrows from
PRIOR = 2.0  # games of 50 % added to every spread


def add(counts: dict, text: str) -> None:
    for line in text.splitlines():
        unseen, spread, points, games = line.split()
        cell = counts[int(unseen)][int(spread)]
        cell[0] += float(points)
        cell[1] += int(games)


def rising(values: list[float], weights: list[float]) -> list[float]:
    """The weighted closest non-decreasing series (pool adjacent violators)."""
    blocks: list[list[float]] = []
    for value, weight in zip(values, weights, strict=True):
        blocks.append([value * weight, weight, 1])
        while len(blocks) > 1 and blocks[-2][0] / blocks[-2][1] > blocks[-1][0] / blocks[-1][1]:
            last = blocks.pop()
            for i in range(3):
                blocks[-1][i] += last[i]
    out: list[float] = []
    for total, weight, length in blocks:
        out += [total / weight] * int(length)
    return out


def fit_row(row: dict) -> list[float]:
    """Win rate at every table spread from {spread: [points, games]}."""
    points = [row.get(s, (0.0, 0))[0] for s in SPREADS]
    games = [row.get(s, (0.0, 0))[1] for s in SPREADS]
    smooth, weights = [], []
    for i in range(len(SPREADS)):
        window = range(max(0, i - WIDTH), min(len(SPREADS), i + WIDTH + 1))
        total = sum(games[j] for j in window)
        smooth.append((sum(points[j] for j in window) + PRIOR / 2) / (total + PRIOR))
        weights.append(total + PRIOR)
    return rising(smooth, weights)


def fit(counts: dict, min_positions: int = MIN_POSITIONS) -> dict[int, list[float]]:
    known = {
        u: fit_row(row)
        for u, row in counts.items()
        if sum(cell[1] for cell in row.values()) >= min_positions
    }
    if not known:
        raise SystemExit("no tile count has enough positions")
    return {u: known[min(known, key=lambda k: (abs(k - u), k))] for u in range(MAX_UNSEEN + 1)}


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("folders", nargs="+", type=Path)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--name", default="winstats.txt", help="name of the counts files")
    p.add_argument("--min-positions", type=int, default=MIN_POSITIONS)
    args = p.parse_args()
    counts: dict = defaultdict(lambda: defaultdict(lambda: [0.0, 0]))
    for folder in args.folders:
        for path in sorted(folder.rglob(args.name)):
            add(counts, path.read_text())
    table = fit(counts, args.min_positions)
    lines = ["unseen,spread,win"]
    for unseen, row in sorted(table.items()):
        lines += [f"{unseen},{s},{w:.4f}" for s, w in zip(SPREADS, row, strict=True)]
    args.out.write_text("\n".join(lines) + "\n")
    print(f"{len(lines) - 1} cells -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
