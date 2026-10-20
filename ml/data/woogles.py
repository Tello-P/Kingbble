"""Downloads finished games from Woogles (woogles.io) as GCG.

Starting from seed players, the crawler lists each player's recent games, saves
every finished classic game not saved yet as <game_id>.gcg plus one line of
metadata in index.jsonl, and queues the human opponents it meets. Requests are
spaced by --delay seconds; rerunning skips what is already on disk.

  python -m ml.data.woogles --players conrad,wellington --max-games 1000 --delay 1.0
"""

from __future__ import annotations

import argparse
import json
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
) -> int:
    """Saves up to max_games new games; returns how many were saved."""
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
                if saved >= max_games:
                    break
    return saved


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--players", required=True, help="comma-separated seed nicknames")
    ap.add_argument("--max-games", type=int, default=1000)
    ap.add_argument("--delay", type=float, default=1.0, help="seconds between requests")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()
    saved = crawl(args.players.split(","), args.max_games, args.out, delay=args.delay)
    print(f"saved {saved} games to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
