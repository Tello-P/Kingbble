"""Exports the turn store as fixed-width binary arrays for training.

For each split, the human turns with a known rack go to <out>/<split>/ as raw
little-endian arrays that np.memmap opens directly; <out>/meta.json gives every
array's dtype and shape, and the tile encoding.

  python -m ml.data.export [--store data/turns] [--out data/memmap] [--lexicon NWL23]

Tiles: 0 none, 1-26 A-Z, 27 blank on a rack, 33-58 a blank played as a-z,
63 a tile already on the board (played through).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from array import array
from pathlib import Path

from ml.data.split import SPLITS
from ml.data.store import DEFAULT_STORE

DEFAULT_OUT = Path(__file__).resolve().parents[2] / "data" / "memmap"
KINDS = ("place", "exchange", "pass")
NO_POSITION = 255
# name: (array typecode, numpy dtype, columns)
ARRAYS = {
    "rack": ("B", "<u1", 7),
    "kind": ("B", "<u1", 1),
    "position": ("B", "<u1", 3),  # row, column, 0 across / 1 down
    "word": ("B", "<u1", 15),
    "exchanged": ("B", "<u1", 7),
    "numbers": ("h", "<i2", 6),  # score, score_before, opp_score_before, bag_before, rating, turn
}


def tile(c: str) -> int:
    if c == "?":
        return 27
    if c == ".":
        return 63
    if "A" <= c <= "Z":
        return ord(c) - ord("A") + 1
    if "a" <= c <= "z":
        return ord(c) - ord("a") + 33
    raise ValueError(f"bad tile {c!r}")


def tiles(s: str, width: int) -> list[int]:
    codes = [tile(c) for c in s][:width]
    return codes + [0] * (width - len(codes))


def position(pos: str) -> list[int]:
    """'8E' (across) or 'E8' (down) to [row, column, direction], 0-based."""
    if not pos:
        return [NO_POSITION] * 3
    m = re.fullmatch(r"(\d+)([A-O])", pos)
    if m:
        return [int(m[1]) - 1, ord(m[2]) - ord("A"), 0]
    m = re.fullmatch(r"([A-O])(\d+)", pos)
    return [int(m[2]) - 1, ord(m[1]) - ord("A"), 1]


def word(w: str) -> str:
    """Played-through tiles written as (XY) become '.'."""
    return re.sub(r"\(([^)]*)\)", lambda m: "." * len(m[1]), w)


def export(
    store: Path = DEFAULT_STORE, out: Path = DEFAULT_OUT, lexicon: str | None = None
) -> dict:
    """Writes the arrays and meta.json; returns the number of turns per split."""
    import duckdb

    con = duckdb.connect()
    con.read_parquet(str(store / "**" / "*.parquet"), hive_partitioning=True).create_view("turns")
    where = "NOT is_bot AND rack <> '' AND NOT phony"
    params = []
    if lexicon:
        where += " AND lexicon = ?"
        params.append(lexicon)
    meta = {
        "tiles": "0 none, 1-26 A-Z, 27 blank, 33-58 blank as a-z, 63 played through",
        "kinds": list(KINDS),
        "lexicon": lexicon,
        "splits": {},
    }
    for split in SPLITS:
        cols = {name: array(code) for name, (code, _, _) in ARRAYS.items()}
        rows = con.execute(
            f"""SELECT rack, kind, position, word, exchanged, score, score_before,
                       opp_score_before, bag_before, coalesce(rating, -1), turn
                FROM turns WHERE {where} AND split = ? ORDER BY game_id, turn""",
            [*params, split],
        ).fetchall()
        for rack, kind, pos, w, exch, *numbers in rows:
            cols["rack"].extend(tiles(rack, 7))
            cols["kind"].append(KINDS.index(kind))
            cols["position"].extend(position(pos))
            cols["word"].extend(tiles(word(w), 15))
            # An exchange of unknown tiles is written as their count.
            cols["exchanged"].extend(tiles("" if exch.isdigit() else exch, 7))
            cols["numbers"].extend(numbers)
        d = out / split
        d.mkdir(parents=True, exist_ok=True)
        files = {}
        for name, (_, dtype, width) in ARRAYS.items():
            col = cols[name]
            if sys.byteorder == "big":
                col.byteswap()
            (d / f"{name}.bin").write_bytes(col.tobytes())
            shape = [len(rows), width]
            files[name] = {"file": f"{split}/{name}.bin", "dtype": dtype, "shape": shape}
        meta["splits"][split] = {"turns": len(rows), "arrays": files}
    out.mkdir(parents=True, exist_ok=True)
    (out / "meta.json").write_text(json.dumps(meta, indent=2) + "\n")
    return {s: v["turns"] for s, v in meta["splits"].items()}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--store", type=Path, default=DEFAULT_STORE)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--lexicon", help="only this lexicon (default: all)")
    args = ap.parse_args()
    print(export(args.store, args.out, args.lexicon))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
