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
    curve = [{"fraction": 0.25, "val_nll": 2.0}, {"fraction": 0.5, "val_nll": 1.9},
             {"fraction": 1.0, "val_nll": 1.85}]
    assert still_dropping(curve)
    curve[2]["val_nll"] = 1.899
    assert not still_dropping(curve)
    assert not still_dropping(curve[:1])
