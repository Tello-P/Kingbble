import json

from ml.data.woogles import crawl, game_metadata, wanted

GCG = "#player1 ann Ann\n#player2 bea Bea\n>ann: AERTXYZ 8E RATE +8 8\n"


def info(game_id, a, b, end="STANDARD", variant="classic", bot=False):
    return {
        "game_id": game_id,
        "players": [{"nickname": a, "rating": "1500"}, {"nickname": b, "is_bot": bot}],
        "game_request": {"lexicon": "NWL23", "rules": {"variant_name": variant}},
        "game_end_reason": end,
    }


class FakeApi:
    """Recorded-style responses keyed by player and game id."""

    def __init__(self):
        self.calls = []
        self.recent = {
            "ann": [
                info("g1", "ann", "bea"),
                info("g2", "ann", "HastyBot"),
                info("g3", "ann", "cid", end="ABORTED"),
            ],
            "bea": [info("g1", "ann", "bea"), info("g4", "bea", "dee", variant="wordsmog")],
            "cid": [info("g5", "cid", "ann")],
        }

    def __call__(self, method, body):
        self.calls.append((method, body))
        if method == "GetRecentGames":
            return {"game_info": self.recent.get(body["username"], [])}
        return {"gcg": GCG}


def test_metadata_reads_both_spellings_and_flags_bots():
    meta = game_metadata(
        {
            "gameId": "x",
            "players": [{"nickname": "BasicBot"}, {"nickname": "me", "isBot": False}],
            "gameRequest": {"lexicon": "CSW21", "rules": {"variantName": "classic"}},
            "gameEndReason": "RESIGNED",
        }
    )
    assert meta["game_id"] == "x" and meta["lexicon"] == "CSW21"
    assert [p["is_bot"] for p in meta["players"]] == [True, False]
    assert wanted(meta)
    assert not wanted({**meta, "end_reason": "ABORTED"})


def test_crawl_saves_finished_classic_games_once(tmp_path):
    api = FakeApi()
    saved = crawl(["ann"], 10, tmp_path, post=api, delay=0, sleep=lambda s: None)
    # g1 (once), g2 (against a bot, still saved) and g5; g3 aborted, g4 not classic.
    assert saved == 3
    assert sorted(p.stem for p in tmp_path.glob("*.gcg")) == ["g1", "g2", "g5"]
    index = [json.loads(line) for line in (tmp_path / "index.jsonl").read_text().splitlines()]
    assert [m["game_id"] for m in index] == ["g1", "g2", "g5"]
    # Bots are not crawled.
    crawled = [b["username"] for m, b in api.calls if m == "GetRecentGames"]
    assert "HastyBot" not in crawled and crawled[0] == "ann"
    # A second run finds nothing new.
    assert crawl(["ann"], 10, tmp_path, post=FakeApi(), delay=0, sleep=lambda s: None) == 0


def test_crawl_stops_at_max_games(tmp_path):
    assert crawl(["ann"], 1, tmp_path, post=FakeApi(), delay=0, sleep=lambda s: None) == 1


def test_crawl_skips_other_lexicons(tmp_path):
    kw = dict(post=FakeApi(), delay=0, sleep=lambda s: None)
    assert crawl(["ann"], 10, tmp_path / "a", lexicon="CSW21", **kw) == 0
    assert crawl(["ann"], 10, tmp_path / "b", lexicon="NWL23", **kw) == 3
