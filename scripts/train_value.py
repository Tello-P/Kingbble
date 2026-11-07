#!/usr/bin/env python3
"""Trains the value model on kingbble_valuedata files and writes it for the engine.

Reads every value.csv under the given folders, keeps the last --holdout share of
each file's rows aside, trains a LightGBM model of the rollout win share from the
side of the player to move, and reports log loss and Brier score on the held-out
rows against two baselines: the mean, and a model of the spread and tiles unseen
only (what a win table knows). Writes the trees in the text format of
engine/include/scrabble/value_model.hpp.

  python scripts/train_value.py resultados-todos --out configs/value_csw24.txt
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

FEATURES = [
    "spread", "unseen", "bag", "scoreless", "rack_size", "leave_value", "rack_vowels",
    "rack_blanks", "rack_s", "q_without_u", "unseen_blanks", "unseen_s", "unseen_vowel_share",
    "board_tiles", "open_triple_word", "open_double_word", "unseen_points", "rack_points",
]  # fmt: skip
TABLE_FEATURES = [0, 1]  # spread and unseen


def load(folders: list[Path], holdout: float):
    train, test = [], []
    for folder in folders:
        for path in sorted(folder.rglob("value.csv")):
            rows = []
            for line in path.read_text().splitlines():
                cells = line.split(",")
                if len(cells) != len(FEATURES) + 2:  # a run stopped in the middle of a line
                    continue
                rows.append([float(c) for c in cells])
            cut = int(len(rows) * (1 - holdout))
            train += rows[:cut]
            test += rows[cut:]
    return train, test


def log_loss(labels, preds) -> float:
    eps = 1e-6
    return -sum(
        y * math.log(min(max(p, eps), 1 - eps)) + (1 - y) * math.log(1 - min(max(p, eps), 1 - eps))
        for y, p in zip(labels, preds, strict=True)
    ) / len(labels)


def brier(labels, preds) -> float:
    return sum((y - p) ** 2 for y, p in zip(labels, preds, strict=True)) / len(labels)


def flatten(tree: dict) -> tuple[list[tuple], list[float]]:
    """A LightGBM tree as (feature, threshold, left, right) nodes and leaf values; a child
    below 0 is leaf ~child."""
    nodes: list = []
    leaves: list[float] = []

    def walk(node: dict) -> int:
        if "split_feature" not in node:
            leaves.append(node["leaf_value"])
            return ~(len(leaves) - 1)
        if node.get("decision_type", "<=") != "<=":
            raise SystemExit("only <= splits are supported")
        index = len(nodes)
        nodes.append(None)
        left = walk(node["left_child"])
        right = walk(node["right_child"])
        nodes[index] = (node["split_feature"], node["threshold"], left, right)
        return index

    walk(tree)
    return nodes, leaves


def export(booster, path: Path) -> None:
    """Writes the booster's trees as value_model.hpp reads them."""
    lines = [f"kingbble-value 1 {len(FEATURES)}"]
    for info in booster.dump_model()["tree_info"]:
        nodes, leaves = flatten(info["tree_structure"])
        lines.append(f"tree {len(nodes)} {len(leaves)}")
        lines += [f"{f} {t!r} {a} {b}" for f, t, a, b in nodes]
        lines.append(" ".join(repr(v) for v in leaves))
    path.write_text("\n".join(lines) + "\n")


def check_engine(engine: str, model: Path, rows: list, expected: list) -> int:
    """1 unless the engine's predictions of `model` match LightGBM's to 1e-6."""
    import subprocess
    import tempfile

    with tempfile.NamedTemporaryFile("w", suffix=".csv", delete=False) as f:
        f.write("\n".join(",".join(repr(x) for x in r) for r in rows) + "\n")
    out = subprocess.run(
        [engine, "--predict", str(model), "--in", f.name],
        capture_output=True,
        text=True,
        check=True,
    )
    got = [float(x) for x in out.stdout.split()]
    worst = max(abs(a - b) for a, b in zip(got, expected, strict=True))
    print(f"engine check: {len(got)} rows, largest difference {worst:.2e}")
    return 0 if worst < 1e-6 else 1


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("folders", nargs="+", type=Path)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--holdout", type=float, default=0.1)
    p.add_argument("--rounds", type=int, default=400)
    p.add_argument("--label", choices=["rollout", "game"], default="rollout")
    p.add_argument("--engine", help="kingbble_valuedata: check its predictions of the model")
    args = p.parse_args()

    import lightgbm as lgb  # only needed here

    train, test = load(args.folders, args.holdout)
    if not train or not test:
        print("no data")
        return 1
    column = len(FEATURES) if args.label == "rollout" else len(FEATURES) + 1
    x_train = [r[: len(FEATURES)] for r in train]
    y_train = [r[column] for r in train]
    x_test = [r[: len(FEATURES)] for r in test]
    y_test = [r[len(FEATURES)] for r in test]  # always judged on the rollout share
    params = {"objective": "cross_entropy", "learning_rate": 0.05, "num_leaves": 31,
              "min_data_in_leaf": 50, "verbose": -1, "num_threads": 2}  # fmt: skip
    model = lgb.train(params, lgb.Dataset(x_train, y_train), args.rounds)
    table_rows = [[r[i] for i in TABLE_FEATURES] for r in x_train]
    table = lgb.train(params, lgb.Dataset(table_rows, y_train), args.rounds)
    mean = sum(y_train) / len(y_train)
    predictions = {
        "mean": [mean] * len(y_test),
        "spread+unseen": list(table.predict([[r[i] for i in TABLE_FEATURES] for r in x_test])),
        "value model": list(model.predict(x_test)),
    }
    print(f"train {len(train)} rows, held out {len(test)}")
    for name, preds in predictions.items():
        print(f"{name}: log loss {log_loss(y_test, preds):.4f}, brier {brier(y_test, preds):.4f}")
    gains = model.feature_importance("gain")
    ranked = sorted(zip(FEATURES, gains, strict=True), key=lambda kv: -kv[1])
    print("importance (gain): " + ", ".join(f"{n} {g:.0f}" for n, g in ranked))
    export(model, args.out)
    print(f"model -> {args.out}")
    if args.engine:
        return check_engine(args.engine, args.out, x_test[:500], predictions["value model"][:500])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
