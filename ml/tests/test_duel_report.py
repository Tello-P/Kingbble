import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("duel_report", ROOT / "scripts" / "duel_report.py")
report = importlib.util.module_from_spec(SPEC)
sys.modules["duel_report"] = report
SPEC.loader.exec_module(report)

LINE = "3 40 47 -20 2 8D_CAT 8H_DOG 3 2 30.5 28.0 -0.05 0.01 -4.5 1.0"


def test_parse_keeps_every_field():
    d = report.parse(LINE, "s")[0]
    assert d.game == ("s", 3)
    assert (d.bag, d.unseen, d.spread, d.seat) == (40, 47, -20, 2)
    assert (d.ours, d.theirs) == ("8D_CAT", "8H_DOG")
    assert (d.win, d.points) == (-0.05, -4.5)


def test_parse_skips_a_line_cut_by_a_stopped_run():
    assert report.parse(LINE + "\n3 40 47", "s") == report.parse(LINE, "s")


def test_groups():
    d = report.parse(LINE, "s")[0]
    assert report.phase(d) == "bag 31+"
    assert report.score_state(d) == "within 50"
    assert report.tiles(d) == "we play more tiles"
    assert report.equity_gap(d) == "static equity within 8"
    assert report.exchange(d) == "play vs play"


def test_group_table_counts_the_loss_per_game():
    rows = report.group_table("t", report.parse(LINE, "s") * 2, report.phase, 2)
    assert "2 positions" in rows[1]
    assert "per game -5.00 win pts" in rows[1]
