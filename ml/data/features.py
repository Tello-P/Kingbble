"""Per-option features of human turns, written by the engine's kingbble_features.

  python -m ml.data.features --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2
                             [--raw data/raw/woogles] [--out data/features] [--top 128]
                             [--word-freq lexica/word_freq.tsv] [--alt-lexicon lexica/ENABLE.txt]

<out>/options.bin holds a header (b"KBFT", uint32 version, feature count and
names length, then the space-separated feature names) and float32 rows (the
turn's top options by static equity, then the played option if it ranks
lower); files without the header are version 1, BASE_FEATURES only.
<out>/turns.tsv has one line per turn: game_id, turn, player, rack, bag,
options (all legal options), chosen (rank of the played option, -1 if we do
not generate it), first_row and rows. <out>/turn_info.tsv is what the engine
was told about each turn (rating, split, bot flag), from the turn store.
"""

from __future__ import annotations

import argparse
import csv
import struct
import subprocess
from dataclasses import dataclass
from pathlib import Path

from ml.data.store import DEFAULT_STORE
from ml.data.woogles import DEFAULT_OUT as DEFAULT_RAW

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT = ROOT / "data" / "features"
DEFAULT_TOOL = ROOT / "build" / "engine" / "kingbble_features"
DEFAULT_WORD_FREQ = ROOT / "lexica" / "word_freq.tsv"
DEFAULT_ALT_LEXICON = ROOT / "lexica" / "ENABLE.txt"
MAGIC = b"KBFT"
# Version 1 files: these columns, in MoveFeatureNames() order.
BASE_FEATURES = ("gap", "score", "leave", "log_rank", "tiles", "bingo", "exchange", "pass")


@dataclass
class Turn:
    game_id: str
    turn: int
    player: int
    rack: str
    bag: int
    options: int
    chosen: int
    first_row: int
    rows: int

    @property
    def chosen_row(self) -> int:
        """Row of the played option within this turn's rows, -1 if it is not there."""
        if self.chosen < 0:
            return -1
        return self.chosen if self.chosen < self.rows else self.rows - 1

    @property
    def top(self) -> int:
        """Rows holding the top options (the played one may follow them)."""
        return self.rows - 1 if self.chosen >= self.rows else self.rows


def header(path: Path) -> tuple[tuple[str, ...], int, int]:
    """(feature names, format version, header bytes) of an options.bin file."""
    with open(path, "rb") as f:
        head = f.read(16)
        if len(head) < 16 or head[:4] != MAGIC:
            return BASE_FEATURES, 1, 0
        version, count, size = struct.unpack("<III", head[4:16])
        names = tuple(f.read(size).decode().split())
    if len(names) != count:
        raise ValueError(f"{path}: {count} features in the header but {len(names)} names")
    return names, version, 16 + size


def load(path: Path = DEFAULT_OUT):
    """(turns, options): the turns and a read-only memmap of shape (rows, features).

    The feature names are header(path / "options.bin")[0].
    """
    import numpy as np

    with open(path / "turns.tsv", newline="") as f:
        turns = [
            Turn(
                r["game_id"],
                *(int(r[k]) for k in ("turn", "player")),
                r["rack"],
                *(int(r[k]) for k in ("bag", "options", "chosen", "first_row", "rows")),
            )
            for r in csv.DictReader(f, delimiter="\t")
        ]
    names, _, offset = header(path / "options.bin")
    rows = ((path / "options.bin").stat().st_size - offset) // (4 * len(names))
    if rows == 0:
        return turns, np.zeros((0, len(names)), dtype="<f4")
    options = np.memmap(
        path / "options.bin", dtype="<f4", mode="r", offset=offset, shape=(rows, len(names))
    )
    return turns, options


def _store_view(store: Path):
    import duckdb

    con = duckdb.connect()
    con.read_parquet(str(store / "**" / "*.parquet"), hive_partitioning=True).create_view("turns")
    return con


def store_turns(store: Path = DEFAULT_STORE) -> dict:
    """(game_id, turn) -> (rating or None, split, is_bot) for every turn in the store."""
    rows = (
        _store_view(store)
        .execute("SELECT game_id, turn, rating, split, is_bot FROM turns")
        .fetchall()
    )
    return {(g, t): (rating, split, bool(bot)) for g, t, rating, split, bot in rows}


def ratings(path: Path = DEFAULT_OUT, store: Path = DEFAULT_STORE) -> dict:
    """(game_id, turn) -> (rating or None, split, is_bot) for the turns in turns.tsv."""
    con = _store_view(store)
    con.read_csv(str(path / "turns.tsv"), delimiter="\t", header=True).create_view("features")
    rows = con.execute(
        """SELECT f.game_id, f.turn, t.rating, t.split, t.is_bot
           FROM features f JOIN turns t ON t.game_id = f.game_id AND t.turn = f.turn"""
    ).fetchall()
    return {(g, t): (rating, split, bool(bot)) for g, t, rating, split, bot in rows}


def write_turn_info(info: dict, path: Path) -> None:
    """The engine's --turn-info file: game_id, turn, rating (0 if unknown), split, is_bot."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        for (game, turn), (rating, split, bot) in sorted(info.items()):
            f.write(f"{game}\t{turn}\t{rating or 0}\t{split}\t{int(bot)}\n")


def build(
    lexicon: Path,
    leaves: Path,
    raw: Path = DEFAULT_RAW,
    out: Path = DEFAULT_OUT,
    top: int = 128,
    tool: Path = DEFAULT_TOOL,
    progress: bool = False,
    store: Path | None = DEFAULT_STORE,
    word_freq: Path | None = DEFAULT_WORD_FREQ,
    alt_lexicon: Path | None = DEFAULT_ALT_LEXICON,
) -> str:
    """Runs the engine; optional inputs that do not exist are left out."""
    cmd = [
        str(tool),
        "--lexicon",
        str(lexicon),
        "--leaves",
        str(leaves),
        "--games",
        str(raw),
        "--out",
        str(out),
        "--top",
        str(top),
    ]
    if store is not None and store.exists():
        write_turn_info(store_turns(store), out / "turn_info.tsv")
        cmd += ["--turn-info", str(out / "turn_info.tsv")]
    if word_freq is not None and word_freq.exists():
        cmd += ["--word-freq", str(word_freq)]
    if alt_lexicon is not None and alt_lexicon.exists():
        cmd += ["--alt-lexicon", str(alt_lexicon)]
    if progress:
        cmd.append("--progress")
    return subprocess.run(cmd, check=True, stdout=subprocess.PIPE, text=True).stdout.strip()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--lexicon", type=Path, required=True)
    ap.add_argument("--leaves", type=Path, required=True)
    ap.add_argument("--raw", type=Path, default=DEFAULT_RAW)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--top", type=int, default=128)
    ap.add_argument("--store", type=Path, default=DEFAULT_STORE)
    ap.add_argument("--word-freq", type=Path, default=DEFAULT_WORD_FREQ)
    ap.add_argument("--alt-lexicon", type=Path, default=DEFAULT_ALT_LEXICON)
    ap.add_argument("--tool", type=Path, default=DEFAULT_TOOL)
    ap.add_argument("--progress", action="store_true")
    args = ap.parse_args()
    print(
        build(
            args.lexicon,
            args.leaves,
            args.raw,
            args.out,
            args.top,
            args.tool,
            args.progress,
            args.store,
            args.word_freq,
            args.alt_lexicon,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
