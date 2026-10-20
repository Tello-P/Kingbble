import json

import pytest

from ml.data.store import build, game_rows, stats

pytest.importorskip("pyarrow")
pytest.importorskip("duckdb")

GAME = """#player1 ann Ann
#player2 HastyBot Hasty
#lexicon NWL23
>ann: AERTXYZ 8E RATE +8 8
>HastyBot: SAAAAAA 8E RATES +5 5
>ann: 9F ET +4 12
"""


def test_game_rows_take_metadata_from_the_index():
    meta = {
        "lexicon": "NWL23",
        "players": [{"nickname": "ann", "rating": "1612?", "is_bot": False}],
    }
    rows = game_rows("g1", GAME, meta, "salt")
    assert all("nickname" not in r for r in rows)
    assert rows[0]["player_id"] == rows[2]["player_id"] != rows[1]["player_id"]
    assert rows[0]["split"] == rows[2]["split"] in ("train", "val", "test")
    assert {r["source"] for r in rows} == {"woogles"}
    assert {r["source"] for r in game_rows("g1", GAME, meta, "salt", "selfplay")} == {"selfplay"}
    assert [r["is_bot"] for r in rows] == [False, True, False]
    assert rows[0]["rating"] == 1612 and rows[1]["rating"] is None
    assert rows[2]["rack"] == ""


def test_build_and_stats(tmp_path):
    raw, store = tmp_path / "raw", tmp_path / "turns"
    raw.mkdir()
    (raw / "g1.gcg").write_text(GAME)
    (raw / "g2.gcg").write_text(GAME.replace("NWL23", "CSW21"))
    (raw / "bad.gcg").write_text("#player1 a A\n")
    (raw / "index.jsonl").write_text(json.dumps({"game_id": "g2", "lexicon": "CSW21"}) + "\n")
    assert build(raw, store, salt="salt") == {"games": 2, "failed": 1, "turns": 6}
    # Per game: ann's two turns are human, one of them without a rack.
    assert stats(store) == [("CSW21", 1, 3, 2, 1), ("NWL23", 1, 3, 2, 1)]
