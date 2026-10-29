import pytest

from ml.train import Config, latest_version, next_version, still_dropping


def test_level_sizes_and_custom_games():
    assert Config(level=0).size() == 1000
    assert Config(level=1).size() == 10000
    assert Config(level=1, games=250).size() == 250


def test_versions_count_up_and_skip_unfinished(tmp_path):
    assert next_version(tmp_path).name == "v001"
    assert latest_version(tmp_path) is None
    (tmp_path / "v001").mkdir()
    (tmp_path / "v001" / "model.txt").write_text("")
    (tmp_path / "v002").mkdir()  # no model: unfinished
    assert next_version(tmp_path).name == "v003"
    assert latest_version(tmp_path).name == "v001"


def test_learning_curve_still_dropping():
    curve = [
        {"fraction": 0.25, "val_nll": 2.0},
        {"fraction": 0.5, "val_nll": 1.9},
        {"fraction": 1.0, "val_nll": 1.85},
    ]
    assert still_dropping(curve)
    curve[2]["val_nll"] = 1.899
    assert not still_dropping(curve)
    assert not still_dropping(curve[:1])


def test_human_games_take_the_lexicon_and_files_on_disk(tmp_path):
    pytest.importorskip("duckdb")
    from ml.data.store import build
    from ml.train import human_games

    raw, store = tmp_path / "raw", tmp_path / "turns"
    raw.mkdir()
    game = "#player1 ann Ann\n#player2 bob Bob\n#lexicon {}\n>ann: AERTXYZ 8E RATE +8 8\n"
    for g, lexicon in (("g1", "CSW24"), ("g2", "NWL23"), ("g3", "CSW24")):
        (raw / f"{g}.gcg").write_text(game.format(lexicon))
    build(raw, store, salt="salt")
    (raw / "g3.gcg").unlink()  # in the store, no longer on disk
    cfg = Config(lexicon="lexica/CSW24.txt", human_raw=str(raw), human_store=str(store))
    assert human_games(cfg) == ["g1"]


def test_uniform_test_nll_averages_log_options(monkeypatch):
    pytest.importorskip("numpy")
    import math

    import ml.data.features as features
    from ml.data.features import Turn
    from ml.train import uniform_test_nll

    turns = [Turn("g", i, 0, "", 80, n, 0, 0, 1) for i, n in enumerate((4, 16, 8))]
    info = {
        ("g", 0): (1500, "test", False),
        ("g", 1): (1500, "test", True),
        ("g", 2): (None, "test", False),
    }
    monkeypatch.setattr(features, "load", lambda path: (turns, None))
    monkeypatch.setattr(features, "ratings", lambda path, store: info)
    human = Config(movers="human")
    assert uniform_test_nll(None, None, human) == pytest.approx((math.log(4) + math.log(8)) / 2)
    assert uniform_test_nll(None, None, Config(movers="bot")) == pytest.approx(math.log(16))
    rated = Config(movers="human", min_rating=1400)
    assert uniform_test_nll(None, None, rated) == pytest.approx((math.log(4) + math.log(8)) / 2)
