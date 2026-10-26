import pytest

from ml.evaluate import match_scores, paired_ci, verdict


def test_paired_ci_uses_complete_pairs_only():
    done = {0: (400, 350), 1: (380, 390), 2: (420, 300), 3: (410, 360), 4: (300, 400)}
    ci = paired_ci(done)
    assert ci["pairs"] == 2 and ci["games"] == 4
    assert ci["win"] == pytest.approx(75.0)
    assert ci["spread"] == pytest.approx((20 + 85) / 2)
    assert ci["win_lo"] < ci["win"] < ci["win_hi"]
    assert paired_ci({0: (1, 0), 1: (0, 1)})["win"] is None


def test_match_scores_read_the_journal(tmp_path):
    path = tmp_path / "match.jsonl.done"
    path.write_text("game0\t120\t400\t350\t0\t1\ngame1\t240\t380\t390\t0\t0\n")
    assert match_scores(path) == {0: (400.0, 350.0), 1: (380.0, 390.0)}
    assert match_scores(tmp_path / "missing") == {}


def test_verdict():
    even = {"win": 50, "win_lo": 40, "win_hi": 60}
    assert verdict({"new": 3.0, "none": 4.0}, even) == "better"
    assert verdict({"new": 3.0, "none": 4.0, "previous": 3.005}, even) == "not better"
    loss = {"win": 40, "win_lo": 35, "win_hi": 45}
    gain = {"win": 60, "win_lo": 55, "win_hi": 65}
    assert verdict({"new": 3.0, "previous": 3.5}, loss) == "worse"
    assert verdict({"new": 3.0, "previous": 2.9}, gain) == "better"
    assert verdict({"new": None, "none": None}, {"win": None}) == "not better"
