import json
import math

import pytest

from ml.eval import tournament as t


def write_results(path, games, manifest=None):
    """games: list of (score a, score b); odd games put a in seat 1."""
    lines = []
    for i, (a, b) in enumerate(games, start=1):
        seat = 1 if i % 2 else 2
        s1, s2 = (a, b) if seat == 1 else (b, a)
        lines.append(f"game {i}: {s1} - {s2}, a in seat {seat}, a 10 moves 1.00 s, b 10 moves 1 s")
    path.write_text("\n".join(lines) + "\n")
    if manifest:
        path.with_name(path.name + ".manifest.json").write_text(json.dumps(manifest))
    return path


def test_read_results_maps_seats(tmp_path):
    p = write_results(tmp_path / "r.txt", [(400, 350), (300, 420)])
    assert t.read_results(p) == {1: (400, 350), 2: (300, 420)}


def test_pairs_need_both_games():
    done = {1: (400, 350), 2: (300, 420), 3: (410, 400)}
    assert t.pairs(done) == [(0.5, (50 - 120) / 2)]


def test_t_distribution_matches_tables():
    assert t.t_quantile(0.975, 10) == pytest.approx(2.228, abs=1e-3)
    assert t.t_quantile(0.975, 1000) == pytest.approx(1.962, abs=1e-3)
    assert t.t_sf2(2.228, 10) == pytest.approx(0.05, abs=1e-3)
    assert t.t_sf2(0.0, 5) == pytest.approx(1.0)


def test_mean_ci():
    mean, lo, hi, p = t.mean_ci([1.0, 2.0, 3.0, 4.0], 0.0)
    half = 3.182 * math.sqrt((5 / 3) / 4)
    assert mean == 2.5
    assert (lo, hi) == pytest.approx((2.5 - half, 2.5 + half), abs=1e-2)
    assert p < 0.05
    assert t.mean_ci([1.0, 1.0, 1.0], 1.0)[3] == 1.0


def test_holm():
    adj = t.holm([0.01, 0.04, None, 0.03])
    assert adj == pytest.approx([0.03, 0.06, None, 0.06])
    assert t.holm([0.5, 0.9]) == pytest.approx([1.0, 1.0])


def test_report_end_to_end(tmp_path):
    strong = [(450, 380), (440, 400)] * 15  # a wins every game
    even = [(400, 380), (380, 400)] * 15  # one win each per deal
    manifest = {
        "tags": {"config": "configs/m7/inference.yaml", "commit": "abcdef1234567", "dirty": "0"},
        "config": {"a": "sim+inf:human+eg", "b": "sim+inf:none+eg", "reproducible": "1"},
    }
    p1 = write_results(tmp_path / "issue_m7_inference.txt", strong, manifest)
    p2 = write_results(tmp_path / "issue_m7_endgame.txt", even)
    p3 = write_results(tmp_path / "issue_m7_tiny.txt", [(400, 300), (300, 400)])
    rows = t.report([p1, p2, p3])
    by = {r.name: r for r in rows}
    inf = by["inference"]
    assert (inf.a, inf.games, inf.win, inf.reproducible) == ("sim+inf:human+eg", 30, 100.0, "1")
    assert inf.significant == "a better"
    assert inf.commit == "abcdef1234"
    assert by["issue_m7_endgame"].win == 50.0
    assert by["issue_m7_endgame"].significant == "no"
    assert by["issue_m7_tiny"].significant == "too few pairs"
    assert by["issue_m7_endgame"].p_win_holm >= by["issue_m7_endgame"].p_win

    out = tmp_path / "out"
    assert (
        t.main([str(p1), str(p2), str(p3), str(tmp_path / "missing.txt"), "--out", str(out)]) == 0
    )
    md = (out / "tournament.md").read_text()
    assert "| inference | `sim+inf:human+eg`" in md
    assert (out / "tournament.csv").read_text().splitlines()[0].startswith("name,a,b,pairs")
    svg = (out / "tournament.svg").read_text()
    assert svg.startswith("<svg") and svg.rstrip().endswith("</svg>")
    assert svg.count("<circle") == 4  # two experiments with 2+ pairs, two panels


def test_no_results(tmp_path, capsys):
    assert t.main([str(tmp_path / "none.txt")]) == 0
    assert "no results" in capsys.readouterr().out
    assert "no experiment" in t.svg([])
