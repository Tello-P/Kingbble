"""MLflow logging of the training menu's runs (local file store under build/mlruns).

One run per train or evaluate step, nested under the run that is active (the
Auto loop's); parameters, metrics (the learning curve with the train turns as
step), the version's files as artifacts and its version and level as tags.
Without mlflow installed (the optional "mlflow" extra) every call is a no-op
after one warning, so the pipeline runs the same.
"""

from __future__ import annotations

import contextlib
import json
import os
import warnings
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MLRUNS = ROOT / "build" / "mlruns"
EXPERIMENT = "kingbble-m6"
_warned = False


def _mlflow():
    global _warned
    try:
        import mlflow
    except ImportError:
        if not _warned:
            warnings.warn(
                "mlflow is not installed: runs are not logged (pip install -e .[mlflow])",
                stacklevel=3,
            )
            _warned = True
        return None
    # The file store needs no database (mlflow-skinny has no SQL backend); MLflow 3
    # keeps it in maintenance mode behind this opt-in.
    os.environ.setdefault("MLFLOW_ALLOW_FILE_STORE", "true")
    if not os.environ.get("MLFLOW_TRACKING_URI"):
        mlflow.set_tracking_uri(MLRUNS.as_uri())
    mlflow.set_experiment(EXPERIMENT)
    return mlflow


@contextlib.contextmanager
def run(name: str, tags: dict | None = None):
    """A run nested under the active one, if any; yields the mlflow module or None."""
    mlflow = _mlflow()
    if mlflow is None:
        yield None
        return
    with mlflow.start_run(run_name=name, nested=mlflow.active_run() is not None, tags=tags):
        yield mlflow


def _params(config: dict) -> dict:
    return {
        k: (json.dumps(v) if isinstance(v, list | tuple | dict) else v)
        for k, v in config.items()
        if v is not None and k != "extra"
    }


def log_training(version: Path) -> None:
    """Logs a version saved by ml.train: config, learning curve and files."""
    config = json.loads((version / "config.json").read_text())
    curve = json.loads((version / "curve.json").read_text())
    with run(
        f"train {version.name}",
        {"step": "train", "version": version.name, "level": str(config.get("level"))},
    ) as mlflow:
        if mlflow is None:
            return
        mlflow.log_params(_params(config))
        for point in curve:
            mlflow.log_metric("val_nll", point["val_nll"], step=point["train_turns"])
        mlflow.log_metric("final_val_nll", curve[-1]["val_nll"])
        mlflow.log_metric("seconds", config.get("seconds", 0))
        for name in ("model.txt", "curve.json", "config.json"):
            mlflow.log_artifact(str(version / name))


def log_evaluation(version: Path) -> None:
    """Logs a version's evaluation.json (ml.evaluate)."""
    result = json.loads((version / "evaluation.json").read_text())
    with run(
        f"evaluate {version.name}",
        {"step": "evaluate", "version": version.name, "verdict": result["verdict"]},
    ) as mlflow:
        if mlflow is None:
            return
        mlflow.log_params(
            {"previous": result.get("previous") or "none", "min_gain": result["min_gain"]}
        )
        metrics = {f"log_loss_{k}": v for k, v in result["log_loss"].items() if v is not None}
        metrics |= {
            f"match_{k}": v
            for k, v in result["match"].items()
            if isinstance(v, int | float) and not isinstance(v, bool)
        }
        metrics["seconds"] = result["seconds"]
        mlflow.log_metrics(metrics)
        mlflow.log_artifact(str(version / "evaluation.json"))


def tag_best(version: Path) -> None:
    """Tags the active run with the best version so far."""
    mlflow = _mlflow()
    if mlflow is not None and mlflow.active_run() is not None:
        mlflow.set_tag("best_version", version.name)
