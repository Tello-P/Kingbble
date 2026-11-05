import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def quarter(path, name):
    """The eight 8-character rows of the premium table in a C++ source."""
    source = (ROOT / path).read_text()
    return re.findall(r'"([TDtd.]{8})"', source[source.index(name) :])[:8]


def test_helper_board_is_the_engine_board():
    # Without its premium squares Quackle scores every move wrongly and plays far
    # below its level; its EnglishBoard is 17 x 13, not this board.
    helper = quarter("scripts/quackle_match/main.cpp", "kQuarter")
    engine = quarter("engine/src/board.cpp", "kQuadrant")
    assert len(helper) == 8
    assert helper == engine


def test_helper_does_not_use_quackles_own_boards():
    source = (ROOT / "scripts/quackle_match/main.cpp").read_text()
    assert "setBoardParameters(StandardBoard())" in source
    assert "EnglishBoard()" not in source
