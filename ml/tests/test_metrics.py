import math

from ml.eval.metrics import accuracy, canonical, ece, log_loss, reliability, summarize


def test_canonical_and_accuracy():
    assert canonical("tea?") == "?AET"
    assert accuracy(["AET", "XY"], ["TEA", "XZ"]) == 0.5


def test_log_loss():
    assert log_loss([1.0, 1.0]) == 0.0
    assert math.isclose(log_loss([0.5, 0.25]), (math.log(2) + math.log(4)) / 2)
    assert math.isfinite(log_loss([0.0]))  # floored, not infinite


def test_ece_is_zero_when_calibrated_and_large_when_not():
    probs = [0.25] * 4 + [0.75] * 4
    labels = [True, False, False, False, True, True, True, False]
    assert ece(probs, labels) == 0.0
    assert math.isclose(ece([0.9] * 10, [False] * 10), 0.9)
    bins = reliability([0.05, 0.95, 1.0], [False, True, True], bins=10)
    assert bins[0] == (1, 0.05, 0.0) and bins[9][0] == 2


def test_summarize_groups_by_model():
    records = [
        {"model": "a", "truth": "AE", "guess": "EA", "p_true": 0.5,
         "tile_probs": {"A": 1.0, "E": 1.0, "Z": 0.0}},
        {"model": "a", "truth": "Z", "guess": "A", "p_true": 0.5,
         "tile_probs": {"A": 0.9, "Z": 0.1}},
        {"model": "b", "truth": "Z", "guess": "Z", "p_true": 1.0, "tile_probs": {"Z": 1.0}},
    ]
    s = summarize(records)
    assert s["a"]["n"] == 2 and s["a"]["accuracy"] == 0.5
    assert math.isclose(s["a"]["log_loss"], math.log(2))
    assert math.isclose(s["a"]["tile_ece"], 0.36)
    assert s["b"] == {"n": 1, "log_loss": 0.0, "accuracy": 1.0, "tile_ece": 0.0}
