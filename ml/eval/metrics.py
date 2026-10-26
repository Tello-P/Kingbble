"""Oracle metrics for opponent-rack inference, scored against the true leave.

In annotated games both racks are known, so each inferred posterior over the
opponent's leave can be scored: log-loss of the true leave, top-1 accuracy of
the most likely leave, and the calibration (ECE) of per-tile probabilities.

  python -m ml.eval.metrics posteriors.jsonl [...]

Each JSONL record is one inference: "model", "truth" and "guess" (leaves as
tile strings, '?' for a blank), "p_true" (posterior probability of the true
leave) and "tile_probs" (tile -> probability that the leave holds at least one,
for every tile the opponent could hold).
"""

from __future__ import annotations

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path

LOG_FLOOR = 1e-12


def canonical(leave: str) -> str:
    """Sorted leave, blanks first, so equal multisets compare equal."""
    return "".join(sorted(leave.upper(), key=lambda c: (c != "?", c)))


def log_loss(p_true: list[float], floor: float = LOG_FLOOR) -> float:
    """Mean negative log probability of the truth (natural log)."""
    if not p_true:
        return math.nan
    return -sum(math.log(max(p, floor)) for p in p_true) / len(p_true)


def accuracy(guesses: list[str], truths: list[str]) -> float:
    """Fraction of exact leave guesses (as multisets)."""
    if not truths:
        return math.nan
    hits = sum(canonical(g) == canonical(t) for g, t in zip(guesses, truths, strict=True))
    return hits / len(truths)


def reliability(probs: list[float], labels: list[bool], bins: int = 10) -> list[tuple]:
    """Per equal-width bin: (count, mean predicted probability, observed frequency)."""
    sums = [[0, 0.0, 0.0] for _ in range(bins)]
    for p, y in zip(probs, labels, strict=True):
        b = sums[min(int(p * bins), bins - 1)]
        b[0] += 1
        b[1] += p
        b[2] += float(y)
    return [(n, s / n, k / n) if n else (0, 0.0, 0.0) for n, s, k in sums]


def ece(probs: list[float], labels: list[bool], bins: int = 10) -> float:
    """Expected calibration error: bin-weighted |confidence - frequency|."""
    total = len(probs)
    if not total:
        return math.nan
    return sum(n * abs(conf - freq) for n, conf, freq in reliability(probs, labels, bins)) / total


def tile_pairs(records: list[dict]) -> tuple[list[float], list[bool]]:
    """(probability, held) for every tile of every record, for calibration."""
    probs, labels = [], []
    for r in records:
        held = set(canonical(r["truth"]))
        for tile, p in r["tile_probs"].items():
            probs.append(float(p))
            labels.append(tile.upper() in held)
    return probs, labels


def summarize(records: list[dict], bins: int = 10) -> dict:
    """Metrics per model: n, log_loss, accuracy and tile_ece."""
    by_model = defaultdict(list)
    for r in records:
        by_model[r.get("model", "")].append(r)
    out = {}
    for model, rs in sorted(by_model.items()):
        probs, labels = tile_pairs(rs)
        out[model] = {
            "n": len(rs),
            "log_loss": log_loss([r["p_true"] for r in rs]),
            "accuracy": accuracy([r["guess"] for r in rs], [r["truth"] for r in rs]),
            "tile_ece": ece(probs, labels, bins),
        }
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("files", type=Path, nargs="+")
    ap.add_argument("--bins", type=int, default=10)
    args = ap.parse_args()
    records = [
        json.loads(line) for f in args.files for line in f.read_text().splitlines() if line.strip()
    ]
    for model, m in summarize(records, args.bins).items():
        print(
            f"{model or '-'}: n={m['n']} log_loss={m['log_loss']:.4f} "
            f"accuracy={m['accuracy']:.4f} tile_ece={m['tile_ece']:.4f}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
