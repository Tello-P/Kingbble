"""Learns RiskBot's risk policy (#53): lambda_scale per bag and lead bucket.

  python -m ml.rl.risk_policy --work build/rl/risk [--arms 0,0.5,1,2,4]
                              [--games 40] [--rounds 3] [--threads 2] [--mem 6G]
                              [--selfplay-args "--sim-candidates 8 --sim-iterations 50"]

A bandit over the scale of each rule of the policy (bag 7-12 / 13-25 x trailing /
leading by default), one rule at a time with the others fixed: every arm plays
--games paired self-play games (risk against sim, kingbble_selfplay --paired),
the better half of the arms go on until they have twice as many, and so on for
--rounds rounds (successive halving); the arm with the best win % (then spread)
over all its games is kept for that rule. Each evaluation keeps its games in
<work>/games/<policy>.txt (kingbble_selfplay --results), so a stopped run goes
on where it was. The result is <work>/policy.txt, read by RiskBot with
--risk-policy, and <work>/log.jsonl with every arm's numbers.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path

from ml.limits import Limits

ROOT = Path(__file__).resolve().parents[2]
SELFPLAY = ROOT / "build" / "engine" / "kingbble_selfplay"
# (bag_lo, bag_hi, lead_lo, lead_hi): the rules whose scale is learned.
DEFAULT_BUCKETS = ((7, 12, -1e9, -1), (7, 12, 0, 1e9), (13, 25, -1e9, -1), (13, 25, 0, 1e9))
GAME = re.compile(r"game (\d+): (-?\d+) - (-?\d+), a in seat ([12])")


@dataclass
class Score:
    games: int = 0
    wins: float = 0.0
    spread: float = 0.0

    @property
    def win(self) -> float:
        return self.wins / self.games if self.games else 0.0

    def key(self) -> tuple[float, float]:
        return (self.win, self.spread / self.games if self.games else 0.0)


def policy_text(buckets: Sequence[tuple], scales: Sequence[float]) -> str:
    lines = ["# bag_lo bag_hi lead_lo lead_hi scale"]
    for (bag_lo, bag_hi, lead_lo, lead_hi), scale in zip(buckets, scales, strict=True):
        lines.append(f"{bag_lo} {bag_hi} {lead_lo:g} {lead_hi:g} {scale:g}")
    return "\n".join(lines) + "\n"


def policy_name(scales: Sequence[float]) -> str:
    return "_".join(f"{s:g}" for s in scales)


def score_results(path: Path) -> Score:
    """Bot a's games in a kingbble_selfplay results file."""
    s = Score()
    if not path.exists():
        return s
    for line in path.read_text().splitlines():
        m = GAME.match(line)
        if not m:
            continue
        s0, s1, seat = int(m[2]), int(m[3]), int(m[4])
        a, b = (s0, s1) if seat == 1 else (s1, s0)
        s.games += 1
        s.wins += 1.0 if a > b else 0.5 if a == b else 0.0
        s.spread += a - b
    return s


def halving(
    arms: Sequence[float], evaluate: Callable[[float, int], Score], games: int, rounds: int
) -> tuple[float, dict[float, Score]]:
    """Successive halving: evaluate(arm, total games) plays up to that many games
    for the arm (resuming) and returns its score; the better half go on with
    twice the games. Returns the best arm and every arm's last score."""
    alive = list(arms)
    scores: dict[float, Score] = {}
    total = games
    for r in range(rounds):
        for arm in alive:
            scores[arm] = evaluate(arm, total)
        if r == rounds - 1 or len(alive) == 1:
            break
        alive.sort(key=lambda a: scores[a].key(), reverse=True)
        alive = alive[: max(1, (len(alive) + 1) // 2)]
        total *= 2
    best = max(alive, key=lambda a: scores[a].key())
    return best, scores


def selfplay_evaluator(
    work: Path,
    buckets: Sequence[tuple],
    scales: list[float],
    rule: int,
    args: argparse.Namespace,
    limits: Limits,
) -> Callable[[float, int], Score]:
    def evaluate(arm: float, total: int) -> Score:
        trial = list(scales)
        trial[rule] = arm
        name = policy_name(trial)
        policy = work / "policies" / f"{name}.txt"
        policy.parent.mkdir(parents=True, exist_ok=True)
        policy.write_text(policy_text(buckets, trial))
        results = work / "games" / f"{name}.txt"
        results.parent.mkdir(parents=True, exist_ok=True)
        cmd = [SELFPLAY, "--lexicon", args.lexicon, "--leaves", args.leaves, "--a", "risk",
               "--b", "sim", "--paired", "--games", total + total % 2, "--seed", args.seed,
               "--threads", limits.threads, "--risk-policy", policy, "--results", results,
               *args.selfplay_args.split()]  # fmt: skip
        limits.run(cmd, stdout=subprocess.DEVNULL)
        return score_results(results)

    return evaluate


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--work", type=Path, default=ROOT / "build" / "rl" / "risk")
    ap.add_argument("--lexicon", default=str(ROOT / "lexica" / "CSW24.txt"))
    ap.add_argument("--leaves", default=str(ROOT / "lexica" / "CSW24.klv2"))
    ap.add_argument("--arms", default="0,0.5,1,2,4", help="lambda_scale values to try")
    ap.add_argument("--games", type=int, default=40, help="paired games per arm in round 1")
    ap.add_argument("--rounds", type=int, default=3)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--selfplay-args", default="--sim-candidates 8 --sim-iterations 50")
    ap.add_argument("--threads", type=int, default=Limits().threads)
    ap.add_argument("--mem", default=Limits().mem)
    args = ap.parse_args()
    limits = Limits(args.threads, args.mem)
    arms = [float(a) for a in args.arms.split(",")]
    buckets = DEFAULT_BUCKETS
    scales = [1.0] * len(buckets)
    log = args.work / "log.jsonl"
    args.work.mkdir(parents=True, exist_ok=True)
    for rule, bucket in enumerate(buckets):
        evaluate = selfplay_evaluator(args.work, buckets, scales, rule, args, limits)
        best, scores = halving(arms, evaluate, args.games, args.rounds)
        scales[rule] = best
        record = {"rule": list(bucket), "best": best,
                  "arms": {f"{a:g}": {"games": s.games, "win": s.win,
                                      "spread": s.spread / max(1, s.games)}
                           for a, s in scores.items()}}  # fmt: skip
        with open(log, "a") as f:
            f.write(json.dumps(record) + "\n")
        print(f"rule {bucket}: scale {best:g} ({scores[best].games} games, "
              f"win {100 * scores[best].win:.1f} %)")  # fmt: skip
    (args.work / "policy.txt").write_text(policy_text(buckets, scales))
    print(f"policy -> {args.work / 'policy.txt'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
