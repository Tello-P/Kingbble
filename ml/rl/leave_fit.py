"""Leave values fitted on self-play (or any GCG games with racks): #49.

  python -m ml.rl.leave_fit --raw build/m6/data/equity_vs_equity_s1/gcg --out leaves_fit.csv
                            [--min-bag 1] [--l2 10] [--shrink 50] [--min-count 1]

For every placement or exchange with a known rack and tiles still to draw, the
leave (the rack less the tiles played or exchanged) is credited with what the
mover gained from then on: the final spread less the spread right after the
move. A ridge regression on the leave's tile counts and duplicated tiles gives
a smooth value for any leave; each leave seen in the games is then its own mean
target shrunk towards that value with weight --shrink (in turns), and the
values are centred so that the empty leave is worth 0. The output is
"LEAVE,value" lines ('?' for a blank), which the engine reads as a leave table
(Leaves::FromFile with a .csv path) and wolges' buildlex can turn into KLV2.
"""

from __future__ import annotations

import argparse
import re
from collections import Counter, defaultdict
from pathlib import Path

from ml.data.gcg import GcgError, parse_gcg, turns
from ml.rl.rest_spread import final_scores

TILES = "ABCDEFGHIJKLMNOPQRSTUVWXYZ?"
MAX_LEAVE = 6


def played_tiles(word: str) -> str:
    """Tiles a placement puts on the board, '?' for a blank (lowercase)."""
    new = re.sub(r"\([^)]*\)", "", word).replace(".", "")
    return "".join("?" if c.islower() else c for c in new)


def leave_of(rack: str, used: str) -> str | None:
    """The rack less the tiles used, as a sorted string; None if they are not on the rack."""
    left = Counter(rack)
    for c in used:
        if left[c] == 0:
            return None
        left[c] -= 1
    return "".join(sorted(left.elements(), key=TILES.index))


def samples(gcg_text: str, min_bag: int = 1) -> list[tuple[str, float]]:
    """(leave, target) for the turns of one game: see the module docstring."""
    final = final_scores(gcg_text)
    if final is None:
        return []
    out = []
    for row in turns(parse_gcg(gcg_text)):
        rack = row["rack"]
        if not rack or row["phony"] or row["bag_before"] < min_bag:
            continue
        if row["kind"] == "place":
            used = played_tiles(row["word"])
        elif row["kind"] == "exchange" and row["exchanged"].isalpha():
            used = row["exchanged"].upper()
        else:
            continue
        leave = leave_of(rack, used)
        if leave is None or len(leave) > MAX_LEAVE:
            continue
        p = row["player"]
        after = row["score_before"] + row["score"] - row["opp_score_before"]
        out.append((leave, float(final[p] - final[1 - p] - after)))
    return out


def features(leave: str) -> list[float]:
    """Tile counts and, per tile, the duplicates beyond the first."""
    c = Counter(leave)
    return [float(c[t]) for t in TILES] + [float(max(0, c[t] - 1)) for t in TILES]


def fit(
    rows: list[tuple[str, float]], l2: float = 10.0, shrink: float = 50.0, min_count: int = 1
) -> dict[str, float]:
    """Leave -> value (see the module docstring)."""
    import numpy as np

    x = np.array([[1.0] + features(leave) for leave, _ in rows])
    y = np.array([target for _, target in rows])
    penalty = l2 * np.eye(x.shape[1])
    penalty[0, 0] = 0.0  # the intercept (the empty leave) is not shrunk
    w = np.linalg.solve(x.T @ x + penalty, x.T @ y)
    intercept = w[0]
    by_leave: dict[str, list[float]] = defaultdict(list)
    for leave, target in rows:
        by_leave[leave].append(target)
    values = {}
    for leave, targets in by_leave.items():
        if not leave or len(targets) < min_count:
            continue
        smooth = float(np.dot(w[1:], features(leave)))
        n = len(targets)
        values[leave] = (n * (float(np.mean(targets)) - intercept) + shrink * smooth) / (n + shrink)
    # Single tiles always get a value, seen or not.
    for t in TILES:
        values.setdefault(t, float(np.dot(w[1:], features(t))))
    return values


def write_csv(values: dict[str, float], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        for leave in sorted(values, key=lambda s: (len(s), [TILES.index(c) for c in s])):
            f.write(f"{leave},{values[leave]:.4f}\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--raw", type=Path, required=True, help="directory of GCG games with racks")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--min-bag", type=int, default=1, help="tiles in the bag before the move")
    ap.add_argument("--l2", type=float, default=10.0)
    ap.add_argument("--shrink", type=float, default=50.0)
    ap.add_argument("--min-count", type=int, default=1)
    args = ap.parse_args()
    rows, games, failed = [], 0, 0
    for path in sorted(args.raw.glob("*.gcg")):
        try:
            rows.extend(samples(path.read_text(), args.min_bag))
            games += 1
        except GcgError:
            failed += 1
    if not rows:
        raise SystemExit("no turns with a known rack")
    values = fit(rows, args.l2, args.shrink, args.min_count)
    write_csv(values, args.out)
    print(f"{games} games ({failed} failed), {len(rows)} turns, {len(values)} leaves -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
