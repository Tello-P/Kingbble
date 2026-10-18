#!/usr/bin/env python3
"""Differential test of the endgame solver against Macondo.

Dumps empty-bag positions from greedy self-play with our solver's value for the
player to move, asks Macondo's endgame solver for the same positions and
compares the values (the spread gained from there to the end with best play).

Requires a build of this repo (kingbble_positions, kingbble_buildkwg), Go and a
Macondo checkout:

  python scripts/compare_endgame.py --macondo ~/src/macondo --count 200 --max-tiles 10
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "scripts" / "macondo_endgame" / "main.go"
# Macondo infers the letter distribution from the lexicon name prefix.
LEX_NAME = "NWL_ENABLE"


def read_pairs(lines: list[str]) -> list[tuple[str, int]]:
    """'<cgp>\\n<value>' pairs."""
    return [(lines[i], int(lines[i + 1])) for i in range(0, len(lines), 2)]


def compare(ours: list[tuple[str, int]], theirs: list[int]) -> dict:
    stats = {"positions": 0, "mismatched": 0, "examples": []}
    for (cgp, a), b in zip(ours, theirs, strict=True):
        stats["positions"] += 1
        if a != b:
            stats["mismatched"] += 1
            if len(stats["examples"]) < 5:
                stats["examples"].append((cgp, a, b))
    return stats


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=True, text=True, **kw)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--macondo", type=Path, default=Path.home() / "src" / "macondo")
    ap.add_argument("--lexicon", type=Path, default=ROOT / "lexica" / "ENABLE.txt")
    ap.add_argument("--build", type=Path, default=ROOT / "build")
    ap.add_argument("--work", type=Path, default=ROOT / "build" / "macondo_compare")
    ap.add_argument("--count", type=int, default=200)
    ap.add_argument("--max-tiles", type=int, default=10)
    ap.add_argument("--seed", type=int, default=1)
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

    helper_src = args.macondo / "cmd" / "kingbble_endgame"
    helper_src.mkdir(exist_ok=True)
    shutil.copy(HELPER, helper_src / "main.go")
    helper = work / "kingbble_endgame"
    run([args.go, "build", "-o", str(helper), "./cmd/kingbble_endgame"], cwd=args.macondo)

    dump = run(
        [
            str(apps / "kingbble_positions"),
            "--lexicon",
            str(args.lexicon),
            "--name",
            LEX_NAME,
            "--count",
            str(args.count),
            "--seed",
            str(args.seed),
            "--endgame",
            "--max-tiles",
            str(args.max_tiles),
            "--solve",
        ],
        capture_output=True,
    ).stdout.splitlines()
    ours = read_pairs(dump)
    cgps = "\n".join(cgp for cgp, _ in ours) + "\n"
    mac = run(
        [str(helper), "-data", str(work / "data"), "-lex", LEX_NAME],
        input=cgps,
        capture_output=True,
    ).stdout.split()
    theirs = [int(v) for v in mac]

    stats = compare(ours, theirs)
    print(f"positions: {stats['positions']}, mismatched values: {stats['mismatched']}")
    for cgp, a, b in stats["examples"]:
        print(f"  {cgp}\n    ours: {a:+d}, macondo: {b:+d}")
    return 1 if stats["mismatched"] else 0


if __name__ == "__main__":
    sys.exit(main())
