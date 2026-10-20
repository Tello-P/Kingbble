"""Turn store: downloaded GCG games as Parquet, queried with DuckDB.

  python -m ml.data.store build   # data/raw/woogles -> data/turns (Parquet, by lexicon)
  python -m ml.data.store stats   # games, turns and human turns with a known rack
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
from pathlib import Path

from ml.data.gcg import GcgError, parse_gcg, turns
from ml.data.pseudonym import load_salt, pseudonym
from ml.data.woogles import BOTS, DEFAULT_OUT as DEFAULT_RAW

DEFAULT_STORE = Path(__file__).resolve().parents[2] / "data" / "turns"


def _rating(value) -> int | None:
    m = re.match(r"\d+", str(value or ""))
    return int(m.group()) if m else None


def game_rows(game_id: str, gcg: str, meta: dict | None, salt: str) -> list[dict]:
    """Turns of one game with its metadata: lexicon, bot flag and rating of the mover.

    The mover's nickname is replaced by its pseudonym (player_id).
    """
    game = parse_gcg(gcg)
    players = {p["nickname"]: p for p in (meta or {}).get("players", [])}
    lexicon = (meta or {}).get("lexicon") or game.lexicon or "unknown"
    rows = []
    for row in turns(game):
        nickname = row.pop("nickname")
        p = players.get(nickname, {})
        row.update(
            player_id=pseudonym(nickname, salt),
            game_id=game_id,
            lexicon=lexicon,
            is_bot=bool(p.get("is_bot", nickname in BOTS)),
            rating=_rating(p.get("rating")),
            end_reason=(meta or {}).get("end_reason", ""),
            created_at=(meta or {}).get("created_at", ""),
        )
        rows.append(row)
    return rows


def build(raw: Path = DEFAULT_RAW, store: Path = DEFAULT_STORE, salt: str | None = None) -> dict:
    """Rewrites the store from every <game_id>.gcg under `raw`; salt defaults to load_salt()."""
    import pyarrow as pa
    import pyarrow.dataset as ds

    salt = salt if salt is not None else load_salt()
    metas = {}
    index = raw / "index.jsonl"
    if index.exists():
        for line in index.read_text().splitlines():
            if line.strip():
                meta = json.loads(line)
                metas[meta["game_id"]] = meta
    rows, games, failed = [], 0, 0
    for path in sorted(raw.glob("*.gcg")):
        try:
            rows.extend(game_rows(path.stem, path.read_text(), metas.get(path.stem), salt))
            games += 1
        except GcgError:
            failed += 1
    if store.exists():
        shutil.rmtree(store)
    if rows:
        ds.write_dataset(
            pa.Table.from_pylist(rows),
            store,
            format="parquet",
            partitioning=["lexicon"],
            partitioning_flavor="hive",
        )
    return {"games": games, "failed": failed, "turns": len(rows)}


def stats(store: Path = DEFAULT_STORE) -> list[tuple]:
    """Per lexicon: games, turns, human turns and human turns with a known rack."""
    import duckdb

    con = duckdb.connect()
    con.read_parquet(str(store / "**" / "*.parquet"), hive_partitioning=True).create_view("turns")
    return con.execute(
        """
        SELECT lexicon,
               count(DISTINCT game_id) AS games,
               count(*) AS turns,
               count(*) FILTER (WHERE NOT is_bot) AS human_turns,
               count(*) FILTER (WHERE NOT is_bot AND rack <> '') AS human_turns_with_rack
        FROM turns GROUP BY lexicon ORDER BY lexicon
        """
    ).fetchall()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command", choices=["build", "stats"])
    ap.add_argument("--raw", type=Path, default=DEFAULT_RAW)
    ap.add_argument("--store", type=Path, default=DEFAULT_STORE)
    args = ap.parse_args()
    if args.command == "build":
        print(build(args.raw, args.store))
        return 0
    total = 0
    for lexicon, games, n, human, with_rack in stats(args.store):
        print(f"{lexicon}: {games} games, {n} turns, {human} human, {with_rack} human with rack")
        total += with_rack
    print(f"human turns with rack: {total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
