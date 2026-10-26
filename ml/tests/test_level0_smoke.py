"""The whole level 0 pipeline at a tiny size (about a minute); skipped without
the built engine and the CSW24 files, as in CI."""

import json
from pathlib import Path

import pytest

from app.cli import main

ROOT = Path(__file__).resolve().parents[2]
NEEDED = [ROOT / "build" / "engine" / t for t in
          ("kingbble_selfplay", "kingbble_features", "kingbble_inference_eval")]
NEEDED += [ROOT / "lexica" / "CSW24.txt", ROOT / "lexica" / "CSW24.klv2"]


@pytest.mark.skipif(not all(p.exists() for p in NEEDED), reason="engine or CSW24 missing")
def test_level0_runs_end_to_end(tmp_path, monkeypatch):
    pytest.importorskip("numpy")
    pytest.importorskip("duckdb")
    monkeypatch.setenv("MLFLOW_TRACKING_URI", (tmp_path / "mlruns").as_uri())
    work = tmp_path / "work"
    assert main(["auto", "--work", str(work), "--games", "30", "--max-level", "0",
                 "--budget-cpu-hours", "1", "--budget-gb", "1", "--threads", "2",
                 "--eval-games", "2", "--eval-sim-candidates", "2", "--eval-sim-iterations",
                 "5", "--eval-max-turns", "20", "--force"]) == 0
    state = json.loads((work / "auto.json").read_text())
    assert state["stopped"] and state["best"]
    best = Path(state["best"])
    assert len(json.loads((best / "curve.json").read_text())) == 3
    e = json.loads((best / "evaluation.json").read_text())
    assert e["beats_uniform"] and e["match"]["games"] == 2
