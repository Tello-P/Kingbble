import json

import pytest

from app.cli import Auto, main, parser
from ml.limits import Limits
from ml.train import Config


@pytest.fixture(autouse=True)
def mlruns(tmp_path, monkeypatch):
    monkeypatch.setenv("MLFLOW_TRACKING_URI", (tmp_path / "mlruns").as_uri())


class Fake:
    """Fake train/evaluate steps: verdicts and curves in order, CPU time per step."""

    def __init__(self, tmp_path, verdicts, dropping, cpu_per_step=60.0):
        self.tmp, self.verdicts, self.dropping = tmp_path, list(verdicts), list(dropping)
        self.cpu, self.step, self.trained = 0.0, cpu_per_step, []

    def train(self, cfg, work, limits, log=print):
        self.cpu += self.step
        v = work / "models" / f"v{len(self.trained) + 1:03d}"
        v.mkdir(parents=True)
        drop = self.dropping.pop(0)
        curve = [{"fraction": 0.5, "val_nll": 2.0}, {"fraction": 1.0, "val_nll": 2.0 - 0.1 * drop}]
        (v / "curve.json").write_text(json.dumps(curve))
        self.trained.append((cfg.level, cfg.top_k, cfg.games))
        return v

    def evaluate(self, version, previous, limits=None, log=print, **kw):
        self.cpu += self.step
        return {"verdict": self.verdicts.pop(0)}

    def auto(self, cpu_hours=10.0, gb=10.0):
        return Auto(Config(level=0, top_k="64"), self.tmp, Limits(1, "none"), cpu_hours, gb,
                    train_fn=self.train, evaluate_fn=self.evaluate, cpu_fn=lambda: self.cpu,
                    disk_fn=lambda p: 0, log=lambda *a: None)


def test_goes_up_while_better_and_still_dropping(tmp_path):
    fake = Fake(tmp_path, ["not better", "better"], [True, False])
    state = fake.auto().run()
    assert fake.trained == [(0, "64", 0), (1, "64", 0)]
    assert state["best"].endswith("v002")
    assert state["stopped"] == "the learning curve no longer drops"


def test_a_losing_version_stops_and_keeps_the_best(tmp_path):
    fake = Fake(tmp_path, ["better", "worse"], [True, True])
    state = fake.auto().run()
    assert state["best"].endswith("v001")
    assert state["stopped"] == "v002 loses to the best version"


def test_stops_at_the_last_level(tmp_path):
    fake = Fake(tmp_path, ["better", "better"], [True, True])
    assert fake.auto().run()["stopped"] == "level 1 is the last one"


def test_cpu_budget_and_resume(tmp_path):
    fake = Fake(tmp_path, ["better", "better"], [True, True], cpu_per_step=1800)
    state = fake.auto(cpu_hours=0.5).run()
    assert state["stopped"] == "CPU budget spent"
    assert state["pending"].endswith("v001")  # trained, not evaluated yet
    # The same command resumes; a stopped loop stays stopped.
    again = fake.auto(cpu_hours=0.5).run()
    assert again == state and len(fake.trained) == 1


def test_disk_budget(tmp_path):
    fake = Fake(tmp_path, [], [])
    auto = fake.auto(gb=1)
    auto.disk_fn = lambda p: 2**31
    assert auto.run()["stopped"] == "disk budget spent"
    assert fake.trained == []


def test_parser_and_status(tmp_path, capsys):
    args = parser().parse_args(["auto", "--budget-cpu-hours", "2", "--budget-gb", "1",
                                "--level", "1", "--min-rating", "1500"])
    assert (args.level, args.min_rating, args.budget_gb) == (1, 1500, 1.0)
    assert main(["status", "--work", str(tmp_path)]) == 0
    assert "disk: 0.00 GB" in capsys.readouterr().out


def test_custom_size_grows_with_the_level(tmp_path):
    fake = Fake(tmp_path, ["better", "better"], [True, True])
    auto = fake.auto()
    auto.cfg = Config(level=0, games=30, top_k="64")
    auto.run()
    assert [games for _, _, games in fake.trained] == [30, 300]
