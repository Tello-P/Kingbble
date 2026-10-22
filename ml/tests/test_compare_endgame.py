import importlib.util
from pathlib import Path

_PATH = Path(__file__).resolve().parents[2] / "scripts" / "compare_endgame.py"
_spec = importlib.util.spec_from_file_location("compare_endgame", _PATH)
ce = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ce)

CGP_A = "/".join(["15"] * 7 + ["5RATES5"] + ["15"] * 7) + " AB/CD 7/0 0 lex X;"
CGP_B = "/".join(["15"] * 7 + ["5RATES5"] + ["15"] * 7) + " EF/GH 7/0 0 lex X;"


def test_read_pairs_parses_values():
    assert ce.read_pairs([CGP_A, "12", CGP_B, "-3"]) == [(CGP_A, 12), (CGP_B, -3)]


def test_compare_counts_mismatched_values():
    stats = ce.compare([(CGP_A, 12), (CGP_B, -3)], [12, -4])
    assert stats["positions"] == 2
    assert stats["mismatched"] == 1
    assert stats["examples"] == [(CGP_B, -3, -4)]


def test_interleave_undoes_a_round_robin_split():
    items = list(range(7))
    assert ce.interleave([items[j::3] for j in range(3)]) == items
    assert ce.interleave([items]) == items


def test_skipped_positions_are_listed_not_compared():
    assert ce.read_pairs([CGP_A, "skip", CGP_B, "4"]) == [(CGP_A, None), (CGP_B, 4)]
    stats = ce.compare([(CGP_A, None), (CGP_B, 4)], [None, 5])
    assert stats["skipped"] == [0]
    assert stats["positions"] == 1 and stats["mismatched"] == 1
    assert ce.compare([(CGP_A, 3)], [None])["skipped"] == [0]
