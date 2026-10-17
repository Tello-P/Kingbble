import importlib.util
from pathlib import Path

_PATH = Path(__file__).resolve().parents[2] / "scripts" / "compare_macondo.py"
_spec = importlib.util.spec_from_file_location("compare_macondo", _PATH)
cm = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cm)

EMPTY = "/".join(["15"] * 15) + " AEINRST/ABCDEFG 0/0 0 lex NWL_ENABLE;"
PLAYED = "/".join(["15"] * 7 + ["5RATES5"] + ["15"] * 7) + " AEINRST/ABCDEFG 7/0 0 lex X;"


def test_parse_move_reads_both_orientations():
    assert cm.parse_move("8D WOr.D (12)") == (7, 3, False, "WOr.D", 12)
    assert cm.parse_move("H10 QI (22)") == (9, 7, True, "QI", 22)


def test_one_tile_plays_match_in_either_direction():
    assert cm.move_key("9F A (2)") == cm.move_key("F9 A (2)")
    assert cm.move_key("8F .A (2)") != cm.move_key("8F .A (3)")
    assert cm.move_key("H7 QI (22)") != cm.move_key("8G QI (22)")


def test_compare_counts_differences():
    ours = cm.read_blocks([EMPTY, "1", "8G QI (22)", PLAYED, "1", "9F A (2)"], True)
    theirs = cm.read_blocks(["1", "8G QI (22)", "1", "9G A (2)"], False)
    stats = cm.compare(ours, theirs)
    assert stats["positions"] == 2
    assert stats["mismatched"] == 1
    assert stats["only_ours"] == 1 and stats["only_macondo"] == 1


def test_compare_flags_duplicates():
    ours = cm.read_blocks([PLAYED, "2", "9F A (2)", "F9 A (2)"], True)
    theirs = cm.read_blocks(["1", "9F A (2)"], False)
    assert cm.compare(ours, theirs)["mismatched"] == 1
