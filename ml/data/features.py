"""Per-option features of human turns, written by the engine's kingbble_features.

  python -m ml.data.features --lexicon lexica/NWL23.kwg --leaves lexica/NWL23.klv2
                             [--raw data/raw/woogles] [--out data/features] [--top 64]

<out>/options.bin holds float32 rows of FEATURES (the turn's top options by
static equity, then the played option if it ranks lower); <out>/turns.tsv has
one line per turn: game_id, turn, player, rack, bag, options (all legal
options), chosen (rank of the played option, -1 if we do not generate it),
first_row and rows.
"""

from __future__ import annotations

import argparse
import csv
import subprocess
from dataclasses import dataclass
from pathlib import Path

from ml.data.store import DEFAULT_STORE
from ml.data.woogles import DEFAULT_OUT as DEFAULT_RAW

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT = ROOT / "data" / "features"
DEFAULT_TOOL = ROOT / "build" / "engine" / "kingbble_features"
# Same order as MoveFeatureNames() in engine/src/features.cpp.
FEATURES = ("gap", "score", "leave", "log_rank", "tiles", "bingo", "exchange", "pass")


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


def load(path: Path = DEFAULT_OUT):
    """(turns, options): the turns and a read-only memmap of shape (rows, len(FEATURES))."""
    import numpy as np

    with open(path / "turns.tsv", newline="") as f:
        turns = [
            Turn(r["game_id"], *(int(r[k]) for k in ("turn", "player")), r["rack"],
                 *(int(r[k]) for k in ("bag", "options", "chosen", "first_row", "rows")))
            for r in csv.DictReader(f, delimiter="\t")
        ]
    size = (path / "options.bin").stat().st_size
    rows = size // (4 * len(FEATURES))
    if rows == 0:
        return turns, np.zeros((0, len(FEATURES)), dtype="<f4")
    options = np.memmap(path / "options.bin", dtype="<f4", mode="r", shape=(rows, len(FEATURES)))
    return turns, options


def ratings(path: Path = DEFAULT_OUT, store: Path = DEFAULT_STORE) -> dict:
    """(game_id, turn) -> (rating or None, split), from the turn store."""
    import duckdb

    con = duckdb.connect()
    con.read_parquet(str(store / "**" / "*.parquet"), hive_partitioning=True).create_view("turns")
    con.read_csv(str(path / "turns.tsv"), delim="\t", header=True).create_view("features")
    rows = con.execute(
        """SELECT f.game_id, f.turn, t.rating, t.split
           FROM features f JOIN turns t ON t.game_id = f.game_id AND t.turn = f.turn"""
    ).fetchall()
    return {(g, t): (rating, split) for g, t, rating, split in rows}


def build(
    lexicon: Path,
    leaves: Path,
    raw: Path = DEFAULT_RAW,
    out: Path = DEFAULT_OUT,
    top: int = 64,
    tool: Path = DEFAULT_TOOL,
    progress: bool = False,
) -> str:
    cmd = [str(tool), "--lexicon", str(lexicon), "--leaves", str(leaves), "--games", str(raw),
           "--out", str(out), "--top", str(top)]
    if progress:
        cmd.append("--progress")
    return subprocess.run(cmd, check=True, stdout=subprocess.PIPE, text=True).stdout.strip()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--lexicon", type=Path, required=True)
    ap.add_argument("--leaves", type=Path, required=True)
    ap.add_argument("--raw", type=Path, default=DEFAULT_RAW)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--top", type=int, default=64)
    ap.add_argument("--tool", type=Path, default=DEFAULT_TOOL)
    ap.add_argument("--progress", action="store_true")
    args = ap.parse_args()
    print(build(args.lexicon, args.leaves, args.raw, args.out, args.top, args.tool, args.progress))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
