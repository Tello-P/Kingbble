#!/usr/bin/env python3
"""Differential test of the move generator against Macondo.

Dumps self-play positions with every placement our engine finds, asks Macondo for
the placements of the same positions and compares both sets move by move.

Requires a build of this repo (kingbble_positions, kingbble_buildkwg), Go and a
Macondo checkout:

  python scripts/compare_macondo.py --macondo ~/src/macondo --count 10000
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "scripts" / "macondo_movegen" / "main.go"
# Macondo infers the letter distribution from the lexicon name prefix.
LEX_NAME = "NWL_ENABLE"


def parse_move(line: str) -> tuple[int, int, bool, str, int]:
    """'8D WORD (12)' -> (row, col, vertical, tiles, score); row first means horizontal."""
    coord, tiles, score = line.split()
    vertical = coord[0].isalpha()
    letter, number = (coord[0], coord[1:]) if vertical else (coord[-1], coord[:-1])
    return int(number) - 1, ord(letter) - ord("A"), vertical, tiles, int(score.strip("()"))


def move_key(line: str) -> tuple:
    """Placed tiles and score, independent of how a one-tile play is oriented."""
    row, col, vertical, tiles, score = parse_move(line)
    dr, dc = (1, 0) if vertical else (0, 1)
    placed = tuple((row + dr * i, col + dc * i, ch) for i, ch in enumerate(tiles) if ch != ".")
    return placed, score


def read_blocks(lines: list[str], with_cgp: bool) -> list[tuple[str, list[str]]]:
    """Splits '<cgp>?\\n<n>\\n<n move lines>' blocks."""
    blocks, i = [], 0
    while i < len(lines):
        cgp = ""
        if with_cgp:
            cgp = lines[i]
            i += 1
        n = int(lines[i])
        blocks.append((cgp, lines[i + 1 : i + 1 + n]))
        i += 1 + n
    return blocks


def compare(ours: list[tuple[str, list[str]]], theirs: list[tuple[str, list[str]]]) -> dict:
    stats = {"positions": 0, "mismatched": 0, "only_ours": 0, "only_macondo": 0, "examples": []}
    for (cgp, our_moves), (_, mac_moves) in zip(ours, theirs, strict=True):
        a = {move_key(m): m for m in our_moves}
        b = {move_key(m): m for m in mac_moves}
        stats["positions"] += 1
        extra, missing = a.keys() - b.keys(), b.keys() - a.keys()
        # Equal sets with different sizes means one side listed a move twice.
        if extra or missing or len(our_moves) != len(mac_moves):
            stats["mismatched"] += 1
            stats["only_ours"] += len(extra)
            stats["only_macondo"] += len(missing)
            if len(stats["examples"]) < 5:
                stats["examples"].append(
                    (cgp, sorted(a[k] for k in extra)[:5], sorted(b[k] for k in missing)[:5])
                )
    return stats


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=True, text=True, **kw)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--macondo", type=Path, default=Path.home() / "src" / "macondo")
    ap.add_argument("--lexicon", type=Path, default=ROOT / "lexica" / "ENABLE.txt")
    ap.add_argument("--build", type=Path, default=ROOT / "build")
    ap.add_argument("--work", type=Path, default=ROOT / "build" / "macondo_compare")
    ap.add_argument("--count", type=int, default=10000)
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

    helper_src = args.macondo / "cmd" / "kingbble_movegen"
    helper_src.mkdir(exist_ok=True)
    shutil.copy(HELPER, helper_src / "main.go")
    helper = work / "kingbble_movegen"
    run([args.go, "build", "-o", str(helper), "./cmd/kingbble_movegen"], cwd=args.macondo)

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
            "--moves",
        ],
        capture_output=True,
    ).stdout.splitlines()
    ours = read_blocks(dump, with_cgp=True)
    cgps = "\n".join(cgp for cgp, _ in ours) + "\n"
    mac = run(
        [str(helper), "-data", str(work / "data"), "-lex", LEX_NAME],
        input=cgps,
        capture_output=True,
    ).stdout.splitlines()
    theirs = read_blocks(mac, with_cgp=False)

    stats = compare(ours, theirs)
    total_ours = sum(len(m) for _, m in ours)
    total_mac = sum(len(m) for _, m in theirs)
    print(f"positions: {stats['positions']}, moves ours/macondo: {total_ours}/{total_mac}")
    print(
        f"mismatched positions: {stats['mismatched']}, only ours: {stats['only_ours']}, "
        f"only macondo: {stats['only_macondo']}"
    )
    for cgp, extra, missing in stats["examples"]:
        print(f"  {cgp}\n    only ours: {extra}\n    only macondo: {missing}")
    return 1 if stats["mismatched"] else 0


if __name__ == "__main__":
    sys.exit(main())
