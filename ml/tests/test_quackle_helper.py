from pathlib import Path

HELPER = Path(__file__).resolve().parents[2] / "scripts" / "quackle_match" / "main.cpp"


def test_helper_plays_on_the_board_with_premium_squares():
    # Quackle's base BoardParameters is its "Empty Board": no premium squares, so
    # every move Quackle weighs is scored wrongly and it plays far below its level.
    source = HELPER.read_text()
    assert "new Quackle::EnglishBoard()" in source
    assert "new Quackle::BoardParameters()" not in source
