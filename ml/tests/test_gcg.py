from pathlib import Path

import pytest

from ml.data.gcg import GcgError, new_tiles, parse_gcg, turns

SAMPLE = Path(__file__).resolve().parents[2] / "lexica" / "fixtures" / "sample.gcg"


def test_parses_headers_and_every_move_type():
    game = parse_gcg(SAMPLE.read_text())
    assert game.players == [("alice", "Alice Smith"), ("bob", "Bob Jones")]
    assert game.lexicon == "ENABLE"
    assert game.game_id == "kingbble sample-1"
    kinds = [ev.kind for ev in game.events]
    assert kinds == [
        "place", "place", "place", "exchange", "place", "phony",
        "place", "challenge", "pass", "exchange", "time", "end_points",
    ]  # fmt: skip
    assert game.events[4].word == "(S)OX"
    assert game.events[9].exchanged == "3"
    assert game.events[-1].rack == "AOX"
    assert game.events[-1].cumulative == 32


def test_turns_track_racks_scores_bag_and_phonies():
    rows = turns(parse_gcg(SAMPLE.read_text()))
    assert [r["kind"] for r in rows] == [
        "place", "place", "place", "exchange", "place", "place", "pass", "exchange",
    ]  # fmt: skip
    first = rows[0]
    assert (first["rack"], first["position"], first["word"]) == ("AERTXYZ", "8E", "RATE")
    assert first["bag_before"] == 86
    assert rows[1]["bag_before"] == 82 and rows[1]["opp_score_before"] == 8
    phony = rows[4]
    assert phony["phony"] and phony["word"] == "(S)OX"
    # The phony's two tiles went back to the bag.
    assert rows[5]["bag_before"] == rows[4]["bag_before"]
    assert rows[5]["score_before"] == 5 and rows[5]["opp_score_before"] == 12


def test_new_tiles_skips_played_through_squares():
    assert new_tiles("RATE") == 4
    assert new_tiles("(S)OX") == 2
    assert new_tiles(".aT") == 2


def test_rejects_unknown_moves_and_players():
    head = "#player1 a A\n#player2 b B\n"
    with pytest.raises(GcgError):
        parse_gcg(head + ">a: ABC ZZ9 FOO +3 3\n")
    with pytest.raises(GcgError):
        turns(parse_gcg(head + ">c: ABC 8D CAB +3 3\n"))
