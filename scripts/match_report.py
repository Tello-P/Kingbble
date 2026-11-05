#!/usr/bin/env python3
"""Sums the results files of a match split in shards.

Reads every match.txt or bank.txt under the given folders (each shard of an
Actions run has one) and prints win %, mean spread, the split by who moved first
and by final margin, and, with --sprt, a sequential test of the win rate.

  python scripts/match_report.py resultados-todos --sprt 0.50 0.53

Both results formats work: selfplay and macondo_match ("game N: S1 - S2, a in seat
K", where S1 is seat 1's score) and quackle_match ("game N: kingbble K - quackle
Q"). Games 2k-1 and 2k share a deal in the first format, and in quackle_match with
--paired (pass --paired here too): the win % interval is then over the pairs of each
shard.
"""

from __future__ import annotations

import argparse
import math
import re
from dataclasses import dataclass
from pathlib import Path
from statistics import mean, stdev

Z95 = 1.96
SEAT_LINE = re.compile(r"game (\d+): (-?\d+) - (-?\d+), a in seat ([12])")
NAMED_LINE = re.compile(r"game (\d+): kingbble (-?\d+) - \w+ (-?\d+)( \(kingbble first\))?")


@dataclass(frozen=True)
class Game:
    shard: str
    number: int
    ours: int
    theirs: int
    first: bool  # we moved first
    seat_line: bool = False  # from a results format whose pairs share a deal

    @property
    def points(self) -> float:
        return 1.0 if self.ours > self.theirs else 0.0 if self.ours < self.theirs else 0.5

    @property
    def spread(self) -> int:
        return self.ours - self.theirs


def parse(lines: list[str], shard: str) -> list[Game]:
    """The games of a results file; a game written twice counts once."""
    games: dict[int, Game] = {}
    for line in lines:
        if m := SEAT_LINE.match(line):
            first = m[4] == "1"
            ours, theirs = (m[2], m[3]) if first else (m[3], m[2])
        elif m := NAMED_LINE.match(line):
            first = m[4] is not None
            ours, theirs = m[2], m[3]
        else:
            continue
        seat_line = m.re is SEAT_LINE
        games[int(m[1])] = Game(shard, int(m[1]), int(ours), int(theirs), first, seat_line)
    return list(games.values())


def load(folders: list[Path]) -> list[Game]:
    games: list[Game] = []
    for folder in folders:
        for name in ("match.txt", "bank.txt"):
            for path in sorted(folder.rglob(name)):
                games += parse(path.read_text().splitlines(), str(path.parent))
    return games


def interval(values: list[float]) -> tuple[float, float]:
    """Mean and its 95 % half-width."""
    if len(values) < 2:
        return (values[0] if values else 0.0, 0.0)
    return mean(values), Z95 * stdev(values) / math.sqrt(len(values))


def pair_points(games: list[Game], named_paired: bool = False) -> list[float]:
    """Mean points of each pair of games sharing a deal, or of each game if unpaired."""
    if not named_paired and not all(g.seat_line for g in games):
        return [g.points for g in games]
    by_game = {(g.shard, g.number): g for g in games}
    pairs = []
    for g in games:
        other = by_game.get((g.shard, g.number + 1))
        if g.number % 2 == 1 and other and g.first != other.first:
            pairs.append((g.points + other.points) / 2)
    return pairs or [g.points for g in games]


def sprt(games: list[Game], p0: float, p1: float, alpha: float, beta: float) -> str:
    """Wald's test on decisive games, in the order they are given; ties are left out."""
    upper, lower = math.log((1 - beta) / alpha), math.log(beta / (1 - alpha))
    llr = 0.0
    for n, g in enumerate(games, 1):
        if g.points == 0.5:
            continue
        llr += math.log(p1 / p0) if g.points == 1 else math.log((1 - p1) / (1 - p0))
        if llr >= upper:
            return f"SPRT: accepts {p1:.0%} after {n} games (LLR {llr:+.2f})"
        if llr <= lower:
            return f"SPRT: accepts {p0:.0%} after {n} games (LLR {llr:+.2f})"
    return (
        f"SPRT: undecided after {len(games)} games (LLR {llr:+.2f}, {lower:+.2f} to {upper:+.2f})"
    )


def line(label: str, games: list[Game], paired: bool = False) -> str:
    points = pair_points(games, paired) if label == "all" else [g.points for g in games]
    win, win_ci = interval(points)
    spread, spread_ci = interval([float(g.spread) for g in games])
    ours, theirs = mean(g.ours for g in games), mean(g.theirs for g in games)
    return (
        f"{label}: {len(games)} games, win {100 * win:.1f} % (95 % CI "
        f"{100 * (win - win_ci):.1f}-{100 * (win + win_ci):.1f}), spread {spread:+.1f} "
        f"(+-{spread_ci:.1f}), points {ours:.0f} - {theirs:.0f}"
    )


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("folders", nargs="+", type=Path)
    p.add_argument("--paired", action="store_true", help="quackle_match ran with --paired")
    p.add_argument("--sprt", nargs=2, type=float, metavar=("P0", "P1"))
    p.add_argument("--alpha", type=float, default=0.05)
    p.add_argument("--beta", type=float, default=0.05)
    args = p.parse_args()
    games = load(args.folders)
    if not games:
        print("no games found")
        return 1
    print(line("all", games, args.paired))
    for label, part in (
        ("moving first", [g for g in games if g.first]),
        ("moving second", [g for g in games if not g.first]),
        ("margin <= 50", [g for g in games if abs(g.spread) <= 50]),
        ("margin > 50", [g for g in games if abs(g.spread) > 50]),
    ):
        if part:
            print(line(label, part))
    if args.sprt:
        print(sprt(games, args.sprt[0], args.sprt[1], args.alpha, args.beta))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
