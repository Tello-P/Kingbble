#!/usr/bin/env python3
"""Differential test of the pre-endgame solver against Macondo.

Dumps pre-endgame positions (1..N tiles in the bag) from greedy self-play with
our solution for the best few plays by static equity, asks Macondo's
pre-endgame solver to analyse the same plays and compares, play by play, the
wins (ties count half, counted in bag permutations) and the tiebreak spreads.
Both sides analyse every play fully (no early cutoff) with exact endgames.

Requires a build of this repo (kingbble_positions, kingbble_buildkwg), Go and a
Macondo checkout:

  python scripts/compare_preendgame.py --macondo ~/src/macondo --count 40 --max-bag 2 --jobs 8
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "scripts" / "macondo_preendgame" / "main.go"
# Macondo infers the letter distribution from the lexicon name prefix.
LEX_NAME = "NWL_ENABLE"


def read_pairs(lines: list[str]) -> list[tuple[str, dict]]:
    """'<cgp>\\n<json solution>' pairs."""
    return [(lines[i], json.loads(lines[i + 1])) for i in range(0, len(lines), 2)]


def compare(ours: list[tuple[str, dict]], theirs: list[dict]) -> dict:
    stats = {"positions": 0, "plays": 0, "mismatched": 0, "examples": []}
    for (cgp, a), b in zip(ours, theirs, strict=True):
        stats["positions"] += 1
        mine = {p["move"]: p for p in a["plays"]}
        mac = {p["move"]: p for p in b["plays"]}
        stats["plays"] += len(mine)
        diffs = []
        if mine.keys() != mac.keys():
            diffs.append(f"plays differ: {sorted(mine.keys() ^ mac.keys())}")
        for move in mine.keys() & mac.keys():
            p, q = mine[move], mac[move]
            if abs(p["wins"] - q["wins"]) > 1e-6 or p["spread"] != q["spread"]:
                diffs.append(
                    f"{move}: ours {p['wins']:g} wins, spread {p['spread']};"
                    f" macondo {q['wins']:g} wins, spread {q['spread']}"
                )
        if diffs:
            stats["mismatched"] += 1
            if len(stats["examples"]) < 5:
                stats["examples"].append((cgp, diffs))
    return stats


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=True, text=True, **kw)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--macondo", type=Path, default=Path.home() / "src" / "macondo")
    ap.add_argument("--lexicon", type=Path, default=ROOT / "lexica" / "ENABLE.txt")
    ap.add_argument("--leaves", type=Path, default=ROOT / "lexica" / "NWL23.klv2")
    ap.add_argument("--build", type=Path, default=ROOT / "build")
    ap.add_argument("--work", type=Path, default=ROOT / "build" / "macondo_compare")
    ap.add_argument("--count", type=int, default=40, help="positions per job")
    ap.add_argument("--max-bag", type=int, default=2)
    ap.add_argument("--candidates", type=int, default=3)
    ap.add_argument("--pass", dest="with_pass", action="store_true", help="also analyse passing")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--jobs", type=int, default=1, help="parallel processes on each side")
    ap.add_argument("--macondo-threads", type=int, default=1, help="threads per Macondo process")
    ap.add_argument("--go", default=shutil.which("go") or str(Path.home() / ".local/go/bin/go"))
    args = ap.parse_args()

    work = args.work
    gaddag_dir = work / "data" / "lexica" / "gaddag"
    gaddag_dir.mkdir(parents=True, exist_ok=True)
    shutil.copytree(
        args.macondo / "data" / "letterdistributions",
        work / "data" / "letterdistributions",
        dirs_exist_ok=True,
    )
    apps = args.build / "engine"
    run([str(apps / "kingbble_buildkwg"), str(args.lexicon), str(gaddag_dir / f"{LEX_NAME}.kwg")])

    helper_src = args.macondo / "cmd" / "kingbble_preendgame"
    helper_src.mkdir(exist_ok=True)
    shutil.copy(HELPER, helper_src / "main.go")
    helper = work / "kingbble_preendgame"
    run([args.go, "build", "-o", str(helper), "./cmd/kingbble_preendgame"], cwd=args.macondo)

    def ours_job(j: int) -> list[tuple[str, dict]]:
        cmd = [
            str(apps / "kingbble_positions"),
            "--lexicon", str(args.lexicon),
            "--leaves", str(args.leaves),
            "--name", LEX_NAME,
            "--count", str(args.count),
            "--seed", str(args.seed + j),
            "--preendgame", str(args.max_bag),
            "--solve", "--exhaustive",
            "--candidates", str(args.candidates),
        ]  # fmt: skip
        if args.with_pass:
            cmd.append("--pass")
        return read_pairs(run(cmd, capture_output=True).stdout.splitlines())

    def macondo_job(chunk: list[tuple[str, dict]]) -> list[dict]:
        lines = "".join(
            cgp + "\t" + ";".join(p["move"] for p in sol["plays"]) + "\n" for cgp, sol in chunk
        )
        out = run(
            [
                str(helper),
                "-data",
                str(work / "data"),
                "-lex",
                LEX_NAME,
                "-no-cutoff",
                "-threads",
                str(args.macondo_threads),
            ],  # fmt: skip
            input=lines,
            capture_output=True,
        ).stdout
        return [json.loads(line) for line in out.splitlines()]

    with ThreadPoolExecutor(args.jobs) as pool:
        start = time.monotonic()
        chunks = list(pool.map(ours_job, range(args.jobs)))
        ours_time = time.monotonic() - start
        start = time.monotonic()
        theirs = [sol for part in pool.map(macondo_job, chunks) for sol in part]
        macondo_time = time.monotonic() - start
    ours = [pair for part in chunks for pair in part]

    stats = compare(ours, theirs)
    print(
        f"positions: {stats['positions']}, plays: {stats['plays']},"
        f" mismatched positions: {stats['mismatched']}"
    )
    print(f"wall time with {args.jobs} jobs: ours {ours_time:.0f} s, macondo {macondo_time:.0f} s")
    for cgp, diffs in stats["examples"]:
        print(f"  {cgp}")
        for d in diffs[:5]:
            print(f"    {d}")
    return 1 if stats["mismatched"] else 0


if __name__ == "__main__":
    sys.exit(main())
