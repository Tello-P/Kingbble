import json
import sys

import pytest

from ml import tracking


def version_dir(tmp_path):
    v = tmp_path / "v001"
    v.mkdir()
    (v / "config.json").write_text(
        json.dumps(
            {"level": 0, "games": 40, "bots": ["a", "b"], "min_rating": None, "seconds": 3.5}
        )
    )
    (v / "curve.json").write_text(
        json.dumps(
            [
                {"fraction": 0.5, "train_turns": 100, "val_turns": 20, "val_nll": 2.0},
                {"fraction": 1.0, "train_turns": 200, "val_turns": 20, "val_nll": 1.9},
            ]
        )
    )
    (v / "model.txt").write_text("version 2\n")
    (v / "evaluation.json").write_text(
        json.dumps(
            {
                "previous": None,
                "min_gain": 0.01,
                "verdict": "better",
                "seconds": 2.0,
                "log_loss": {"none": 4.5, "new": 3.3, "turns": 21},
                "match": {"pairs": 1, "games": 2, "win": None, "opponent": "none"},
            }
        )
    )
    return v


def test_runs_nest_under_the_active_run(tmp_path, monkeypatch):
    mlflow = pytest.importorskip("mlflow")
    monkeypatch.setenv("MLFLOW_TRACKING_URI", (tmp_path / "mlruns").as_uri())
    v = version_dir(tmp_path)
    with tracking.run("auto") as m:
        parent = m.active_run().info.run_id
        tracking.log_training(v)
        tracking.log_evaluation(v)
        tracking.tag_best(v)
    assert list((tmp_path / "mlruns").rglob("model.txt"))
    runs = mlflow.search_runs(experiment_names=[tracking.EXPERIMENT], output_format="list")
    by_name = {r.info.run_name: r for r in runs}
    train, evaluate = by_name["train v001"], by_name["evaluate v001"]
    assert train.data.tags["mlflow.parentRunId"] == parent
    assert train.data.metrics["final_val_nll"] == pytest.approx(1.9)
    assert train.data.params["games"] == "40" and "min_rating" not in train.data.params
    assert evaluate.data.metrics["log_loss_new"] == pytest.approx(3.3)
    assert evaluate.data.tags["verdict"] == "better"
    assert by_name["auto"].data.tags["best_version"] == "v001"


def test_without_mlflow_nothing_is_logged(tmp_path, monkeypatch):
    monkeypatch.setitem(sys.modules, "mlflow", None)
    monkeypatch.setattr(tracking, "_warned", False)
    v = version_dir(tmp_path)
    with pytest.warns(UserWarning, match="mlflow is not installed"):
        tracking.log_training(v)
    tracking.log_evaluation(v)
    tracking.tag_best(v)
