"""Spread still to come after a simulation's playouts, by bag size.

  python -m ml.rl.rest_spread [--store data/turns] [--raw data/raw/woogles]
                              [--lexicon CSW24] [--min-rating 0] [--plies 3]

The sim bot's playouts cover the move and the next two (--plies 3), so a
candidate's final margin is its simulated margin plus what happens after them.
For every turn of the games that ended normally, the residual is the mover's
final margin (from the GCG's last cumulative scores) minus their margin --plies
turns later (0 when the game ended before). Prints the residual standard
deviation per bag size and the fit sd = sd0 + sd_per_sqrt_tile * sqrt(bag)
over bags of 1 or more tiles, the defaults of WinParams.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import duckdb

from ml.data.gcg import parse_gcg
from ml.data.store import DEFAULT_STORE
from ml.data.woogles import DEFAULT_OUT as DEFAULT_RAW


def final_scores(gcg_text: str) -> list[int] | None:
    """Each player's last cumulative score, in seat order."""
    game = parse_gcg(gcg_text)
    seat = {nick: i for i, (nick, _) in enumerate(game.players)}
    scores: list[int | None] = [None, None]
    for ev in game.events:
        if ev.nickname in seat:
            scores[seat[ev.nickname]] = ev.cumulative
    return None if None in scores else scores


def residuals(rows: list[tuple[int, int, int, int]], final: list[int], plies: int):
    """(bag, residual) per turn of one game.

    rows: (player, score_before, opp_score_before, bag_before) in turn order.
    """

    def margin(i: int, p: int) -> int:
        player, mine, theirs, _ = rows[i]
        return mine - theirs if player == p else theirs - mine

    out = []
    for i, (p, _, _, bag) in enumerate(rows):
        end = final[p] - final[1 - p]
        later = margin(i + plies, p) if i + plies < len(rows) else end
        out.append((bag, end - later))
    return out


def fit(points: list[tuple[int, float, int]]) -> tuple[float, float]:
    """Weighted least squares of sd = sd0 + sd_per_sqrt_tile * sqrt(bag) over (bag, sd, n)."""
    sw = sx = sy = sxx = sxy = 0.0
    for bag, sd, n in points:
        x = math.sqrt(bag)
        sw += n
        sx += n * x
        sy += n * sd
        sxx += n * x * x
        sxy += n * x * sd
    slope = (sw * sxy - sx * sy) / (sw * sxx - sx * sx)
    return (sy - slope * sx) / sw, slope


def by_bag(pairs) -> dict[int, tuple[float, int]]:
    """Residual variance (mean square) and count per bag size."""
    acc: dict[int, list[float]] = {}
    for bag, r in pairs:
        a = acc.setdefault(bag, [0.0, 0])
        a[0] += r * r
        a[1] += 1
    return {bag: (s / n, n) for bag, (s, n) in sorted(acc.items())}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--store", type=Path, default=DEFAULT_STORE)
    ap.add_argument("--raw", type=Path, default=DEFAULT_RAW)
    ap.add_argument("--lexicon", default="CSW24")
    ap.add_argument("--min-rating", type=int, default=0)
    ap.add_argument("--plies", type=int, default=3)
    args = ap.parse_args()

    parquet = args.store / f"lexicon={args.lexicon}" / "*.parquet"
    rows = duckdb.sql(
        f"""select game_id, player, score_before, opp_score_before, bag_before
            from '{parquet}'
            where end_reason = 'STANDARD' and game_id in (
              select game_id from '{parquet}' group by 1 having min(rating) >= {args.min_rating})
            order by game_id, turn"""
    ).fetchall()
    games: dict[str, list[tuple[int, int, int, int]]] = {}
    for gid, *rest in rows:
        games.setdefault(gid, []).append(tuple(rest))

    pairs, used = [], 0
    for gid, turns in games.items():
        path = args.raw / f"{gid}.gcg"
        if not path.exists():
            continue
        final = final_scores(path.read_text())
        if final is None:
            continue
        pairs += residuals(turns, final, args.plies)
        used += 1

    table = by_bag(pairs)
    print(f"games: {used}, turns: {len(pairs)}, plies: {args.plies}")
    print("bag\tturns\tsd")
    for bag, (var, n) in table.items():
        print(f"{bag}\t{n}\t{math.sqrt(var):.1f}")
    sd0, per_sqrt = fit(
        [(bag, math.sqrt(var), n) for bag, (var, n) in table.items() if bag >= 1 and n >= 50]
    )
    print(f"fit: sd = {sd0:.2f} + {per_sqrt:.2f} * sqrt(bag)")


if __name__ == "__main__":
    main()
