import importlib.util
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("train_value", ROOT / "scripts" / "train_value.py")
train = importlib.util.module_from_spec(SPEC)
sys.modules["train_value"] = train
SPEC.loader.exec_module(train)


def test_feature_list_matches_the_engine():
    header = (ROOT / "engine/include/scrabble/value_model.hpp").read_text()
    assert f"kValueFeatures = {len(train.FEATURES)};" in header


def test_load_holds_out_the_tail_and_skips_cut_lines(tmp_path):
    row = ",".join(["1"] * len(train.FEATURES)) + ",0.5,1"
    (tmp_path / "value.csv").write_text("\n".join([row] * 10) + "\n1,2")
    fit, held = train.load([tmp_path], 0.2)
    assert len(fit) == 8 and len(held) == 2


def test_metrics():
    assert train.brier([1, 0], [1, 0]) == 0
    assert train.log_loss([1], [0.5]) == pytest.approx(0.6931, abs=1e-3)


def test_export_writes_what_the_engine_reads(tmp_path):
    lgb = pytest.importorskip("lightgbm")
    np = pytest.importorskip("numpy")
    x = np.array([[float(i % 7) - 3] + [0.0] * (len(train.FEATURES) - 1) for i in range(400)])
    y = (x[:, 0] > 0).astype(float)
    model = lgb.train({"objective": "cross_entropy", "verbose": -1, "min_data_in_leaf": 5},
                      lgb.Dataset(x, y), 5)  # fmt: skip
    out = tmp_path / "m.txt"
    train.export(model, out)
    lines = out.read_text().splitlines()
    assert lines[0] == f"kingbble-value 1 {len(train.FEATURES)}"
    assert sum(line.startswith("tree ") for line in lines) == 5


def test_flatten_numbers_nodes_and_leaves():
    tree = {
        "split_feature": 0, "threshold": 1.5, "decision_type": "<=",
        "left_child": {"leaf_value": -0.5},
        "right_child": {"split_feature": 2, "threshold": 0.0, "decision_type": "<=",
                        "left_child": {"leaf_value": 0.1}, "right_child": {"leaf_value": 0.9}},
    }  # fmt: skip
    nodes, leaves = train.flatten(tree)
    assert nodes == [(0, 1.5, -1, 1), (2, 0.0, -2, -3)]
    assert leaves == [-0.5, 0.1, 0.9]
