#!/usr/bin/env python3
"""Sums the position duels of a run: where Kingbble's move and Macondo's differ.

Reads every duel.txt under the given folders (macondo_match --duel). A negative
difference means Macondo's move did better when played out. Each table shows how
many positions fall in a group, the mean differences, and how much of Kingbble's
total loss per game comes from that group.

  python scripts/duel_report.py resultados-todos
"""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass
from pathlib import Path

Z95 = 1.96


@dataclass(frozen=True)
class Duel:
    game: tuple
    bag: int
    unseen: int
    spread: int
    seat: int
    ours: str
    theirs: str
    tiles_ours: int
    tiles_theirs: int
    equity_ours: float
    equity_theirs: float
    win: float
    win_se: float
    points: float
    points_se: float


def parse(text: str, shard: str) -> list[Duel]:
    duels = []
    for line in text.splitlines():
        f = line.split()
        if len(f) != 15:  # a run stopped in the middle of a line
            continue
        duels.append(
            Duel(
                (shard, int(f[0])), int(f[1]), int(f[2]), int(f[3]), int(f[4]), f[5], f[6],
                int(f[7]), int(f[8]), float(f[9]), float(f[10]), float(f[11]), float(f[12]),
                float(f[13]), float(f[14]),
            )
        )  # fmt: skip
    return duels


def load(folders: list[Path]) -> list[Duel]:
    duels: list[Duel] = []
    for folder in folders:
        for path in sorted(folder.rglob("duel.txt")):
            duels += parse(path.read_text(), str(path.parent))
    return duels


def phase(d: Duel) -> str:
    return "bag 1-7" if d.bag <= 7 else "bag 8-30" if d.bag <= 30 else "bag 31+"


def score_state(d: Duel) -> str:
    if d.spread < -50:
        return "behind > 50"
    if d.spread > 50:
        return "ahead > 50"
    return "within 50"


def tiles(d: Duel) -> str:
    if d.tiles_ours > d.tiles_theirs:
        return "we play more tiles"
    if d.tiles_ours < d.tiles_theirs:
        return "we play fewer tiles"
    return "same tiles played"


def equity_gap(d: Duel) -> str:
    gap = d.equity_ours - d.equity_theirs
    if gap > 8:
        return "we are > 8 up on static equity"
    if gap < -8:
        return "we are > 8 down on static equity"
    return "static equity within 8"


def bag_out(d: Duel) -> str:
    if d.bag > 7:
        return "bag over 7"
    ours, theirs = d.tiles_ours >= d.bag, d.tiles_theirs >= d.bag
    if ours and theirs:
        return "both empty the bag"
    if ours:
        return "only we empty the bag"
    return "only Macondo empties the bag" if theirs else "neither empties the bag"


def exchange(d: Duel) -> str:
    def kind(move: str) -> str:
        return move.split("_")[0] if move.split("_")[0] in ("exchange", "pass") else "play"

    return f"{kind(d.ours)} vs {kind(d.theirs)}"


def group_table(title: str, duels: list[Duel], key, games: int) -> list[str]:
    groups: dict[str, list[Duel]] = {}
    for d in duels:
        groups.setdefault(key(d), []).append(d)
    rows = [title]
    for name, part in sorted(groups.items(), key=lambda kv: sum(d.win for d in kv[1])):
        n = len(part)
        win = sum(d.win for d in part) / n
        spread = sum((d.win - win) ** 2 for d in part) / (n - 1) if n > 1 else 0.0
        err = Z95 * math.sqrt(spread / n)  # between positions, not only within rollouts
        points = sum(d.points for d in part) / n
        rows.append(
            f"  {name}: {n} positions, win {100 * win:+.2f} (+-{100 * err:.2f}), "
            f"spread {points:+.2f}, per game {100 * sum(d.win for d in part) / games:+.2f} win pts"
        )
    return rows


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("folders", nargs="+", type=Path)
    p.add_argument("--worst", type=int, default=15, help="positions to list")
    args = p.parse_args()
    duels = load(args.folders)
    if not duels:
        print("no duels found")
        return 1
    games = len({d.game for d in duels})
    n = len(duels)
    print(f"{n} positions where the moves differ, {games} games, {n / games:.1f} per game")
    print(f"mean win {100 * sum(d.win for d in duels) / n:+.2f} pts per position, ")
    print(f"sum per game {100 * sum(d.win for d in duels) / games:+.2f} win pts")
    for title, key in (
        ("by phase", phase),
        ("by exact bag", lambda d: f"bag {d.bag:02d}" if d.bag <= 8 else "bag 09+"),
        ("by who empties the bag", bag_out),
        ("by score", score_state),
        ("by tiles played", tiles),
        ("by static equity", equity_gap),
        ("by kind of move", exchange),
        ("by seat", lambda d: f"seat {d.seat}"),
    ):
        print("\n".join(group_table(title, duels, key, games)))
    print("worst positions (win, spread, bag, score, our move, theirs)")
    for d in sorted(duels, key=lambda d: d.win)[: args.worst]:
        print(f"  {100 * d.win:+.0f} {d.points:+.0f} {d.bag} {d.spread:+d} {d.ours} {d.theirs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
