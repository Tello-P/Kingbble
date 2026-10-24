"""Downloads finished games from Woogles (woogles.io) as GCG.

Starting from seed players, the crawler lists each player's recent games, saves
every finished classic game not saved yet as <game_id>.gcg plus one line of
metadata in index.jsonl, and queues the human opponents it meets. Requests are
spaced by --delay seconds; rerunning skips what is already on disk.

  python -m ml.data.woogles --players conrad,wellington --max-games 1000 --delay 1.0

--max-games is the number of games wanted on disk, so a rerun only tops it up.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.request
from collections import deque
from collections.abc import Callable
from pathlib import Path

API = "https://woogles.io/api/game_service.GameMetadataService/"
DEFAULT_OUT = Path(__file__).resolve().parents[2] / "data" / "raw" / "woogles"
# Woogles' own bots, for games whose metadata does not flag them.
BOTS = {"BasicBot", "BetterBot", "BestBot", "STEEBot", "HastyBot", "MacondoBot"}
UNFINISHED = {"NONE", "ABORTED", "CANCELLED"}

Post = Callable[[str, dict], dict]


def http_post(method: str, body: dict) -> dict:
    req = urllib.request.Request(
        API + method,
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json", "User-Agent": "kingbble-research"},
    )
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read())


def _get(d: dict, *names: str, default=None):
    """First present key among snake_case and camelCase spellings."""
    for n in names:
        if n in d:
            return d[n]
    return default


def game_metadata(info: dict) -> dict:
    """The fields we keep from one GetRecentGames entry."""
    request = _get(info, "game_request", "gameRequest", default={}) or {}
    rules = _get(request, "rules", default={}) or {}
    players = []
    for p in _get(info, "players", default=[]) or []:
        nickname = _get(p, "nickname", default="")
        players.append(
            {
                "nickname": nickname,
                "rating": _get(p, "rating", default=""),
                "is_bot": bool(_get(p, "is_bot", "isBot", default=False)) or nickname in BOTS,
                "score": _get(p, "score", default=None),
            }
        )
    return {
        "game_id": _get(info, "game_id", "gameId", default=""),
        "lexicon": _get(request, "lexicon", default=""),
        "variant": _get(rules, "variant_name", "variantName", default=""),
        "end_reason": _get(info, "game_end_reason", "gameEndReason", default=""),
        "created_at": _get(info, "created_at", "createdAt", default=""),
        "players": players,
    }


def wanted(meta: dict) -> bool:
    return (
        bool(meta["game_id"])
        and meta["variant"] in ("", "classic")
        and meta["end_reason"] not in UNFINISHED
        and len(meta["players"]) == 2
    )


def crawl(
    seeds: list[str],
    max_games: int,
    out: Path = DEFAULT_OUT,
    post: Post = http_post,
    delay: float = 1.0,
    games_per_player: int = 50,
    sleep: Callable[[float], None] = time.sleep,
    on_save: Callable[[int], None] | None = None,
    lexicon: str | None = None,
) -> int:
    """Saves up to max_games new games; returns how many were saved.

    on_save, if given, is called with the running count after each saved game.
    With a lexicon, games in other lexicons are skipped and their players are
    not queued.
    """
    out.mkdir(parents=True, exist_ok=True)
    have = {p.stem for p in out.glob("*.gcg")}
    queue, seen = deque(seeds), set(seeds)
    saved = 0
    with (out / "index.jsonl").open("a") as index:
        while queue and saved < max_games:
            player = queue.popleft()
            listing = post(
                "GetRecentGames",
                {"username": player, "num_games": games_per_player, "offset": 0},
            )
            sleep(delay)
            for info in _get(listing, "game_info", "gameInfo", default=[]) or []:
                meta = game_metadata(info)
                if lexicon and meta["lexicon"] != lexicon:
                    continue
                for p in meta["players"]:
                    if not p["is_bot"] and p["nickname"] and p["nickname"] not in seen:
                        seen.add(p["nickname"])
                        queue.append(p["nickname"])
                if not wanted(meta) or meta["game_id"] in have:
                    continue
                gcg = post("GetGCG", {"game_id": meta["game_id"]}).get("gcg", "")
                sleep(delay)
                if not gcg:
                    continue
                (out / f"{meta['game_id']}.gcg").write_text(gcg)
                index.write(json.dumps(meta) + "\n")
                index.flush()
                have.add(meta["game_id"])
                saved += 1
                if on_save:
                    on_save(saved)
                if saved >= max_games:
                    break
    return saved


def count_saved(out: Path, lexicon: str | None = None) -> int:
    """Games already in out/index.jsonl, in `lexicon` if given."""
    index = out / "index.jsonl"
    if not index.exists():
        return 0
    metas = [json.loads(line) for line in index.read_text().splitlines() if line.strip()]
    return sum(1 for m in metas if not lexicon or m.get("lexicon") == lexicon)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--players", required=True, help="comma-separated seed nicknames")
    ap.add_argument(
        "--max-games", type=int, default=1000, help="target games on disk (in --lexicon)"
    )
    ap.add_argument("--delay", type=float, default=1.0, help="seconds between requests")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--lexicon", default="CSW24", help='only this lexicon; "" for all')
    ap.add_argument("--progress", action="store_true", help='"progress: N/max" on stderr')
    args = ap.parse_args()

    lexicon = args.lexicon or None
    have = count_saved(args.out, lexicon)

    def report(saved: int) -> None:
        print(f"progress: {have + saved}/{args.max_games}", file=sys.stderr, flush=True)

    if args.progress:
        report(0)
    saved = crawl(
        args.players.split(","),
        max(0, args.max_games - have),
        args.out,
        delay=args.delay,
        on_save=report if args.progress else None,
        lexicon=lexicon,
    )
    print(f"saved {saved} new games to {args.out} ({have + saved} in total)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
