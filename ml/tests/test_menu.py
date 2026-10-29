import json
import sys

from app import menu
from app.cli import parser

SETTINGS = {
    "work": "/tmp/w",
    "source": "human",
    "level": 1,
    "games": 0,
    "min_rating": 1500,
    "max_rating": 0,
    "mix": 0.0,
    "mix_raw": "",
    "k": "64",
    "threads": 2,
    "budget_cpu_hours": 3.0,
    "budget_gb": 20.0,
    "max_level": 1,
}


def test_cli_args_parse_with_the_cli_parser():
    for command in menu.COMMANDS:
        args = parser().parse_args(menu.cli_args(command, SETTINGS))
        assert args.command == command and args.threads == 2
    auto = parser().parse_args(menu.cli_args("auto", SETTINGS))
    assert (auto.level, auto.min_rating, auto.budget_gb, auto.max_level) == (1, 1500, 20.0, 1)
    assert "--games" in menu.cli_args("train", SETTINGS | {"games": 500})


def test_versions_and_auto_state(tmp_path):
    v = tmp_path / "models" / "v001"
    v.mkdir(parents=True)
    (v / "config.json").write_text(
        json.dumps({"level": 0, "games_played": 30, "test_nll_logit": 3.7})
    )
    (v / "curve.json").write_text(
        json.dumps([{"fraction": 1.0, "train_turns": 10, "val_nll": 3.6}])
    )
    (v / "evaluation.json").write_text(json.dumps({"verdict": "better", "log_loss": {"new": 5.7}}))
    (tmp_path / "models" / "v002.tmp").mkdir()
    rows = menu.versions(tmp_path)
    assert [r["version"] for r in rows] == ["v001"]
    assert (
        rows[0]["val_nll"] == 3.6 and rows[0]["verdict"] == "better" and rows[0]["test_nll"] == 3.7
    )
    assert menu.auto_state(tmp_path) is None
    (tmp_path / "auto.json").write_text('{"level": 0}')
    assert menu.auto_state(tmp_path) == {"level": 0}


def test_background_job_start_running_and_stop(tmp_path, monkeypatch):
    calls = []

    class Proc:
        pid = 4242

    def fake_popen(cmd, **kw):
        calls.append(cmd)
        return Proc()

    monkeypatch.setattr(menu.subprocess, "Popen", fake_popen)
    s = SETTINGS | {"work": str(tmp_path)}
    assert menu.start("train", s) == 4242
    assert calls[0][:4] == [sys.executable, "-u", "-m", "app.cli"]
    pid_file, log_file = menu.job_files(tmp_path)
    assert pid_file.read_text().split() == ["4242", "train"]
    assert "app.cli train" in log_file.read_text()
    monkeypatch.setattr(menu.os, "kill", lambda pid, sig: None)
    assert menu.running(tmp_path) == 4242
    killed = []
    monkeypatch.setattr(menu.os, "killpg", lambda pid, sig: killed.append(pid))
    assert menu.stop(tmp_path) and killed == [4242]
    pid_file.write_text("nonsense")
    assert menu.running(tmp_path) is None
