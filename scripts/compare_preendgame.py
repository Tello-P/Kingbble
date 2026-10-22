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

With --results FILE each finished position is kept in FILE: stop the run any
time and rerun the same command to go on (--fresh starts over, --status only
reports).
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from ml.eval.results_log import ResultsLog  # noqa: E402
HELPER = ROOT / "scripts" / "macondo_preendgame" / "main.go"
# Macondo infers the letter distribution from the lexicon name prefix.
LEX_NAME = "NWL_ENABLE"


def read_pairs(lines: list[str]) -> list[tuple[str, dict]]:
    """'<cgp>\\n<json solution>' pairs."""
    return [(lines[i], json.loads(lines[i + 1])) for i in range(0, len(lines), 2)]


def compare(ours: list[tuple[str, dict]], theirs: list[dict]) -> dict:
    stats = {"positions": 0, "plays": 0, "mismatched": 0, "examples": [], "skipped": []}
    for i, ((cgp, a), b) in enumerate(zip(ours, theirs, strict=True)):
        # {"skip": true}: not solved within the time limit on that side.
        if a.get("skip") or b.get("skip"):
            stats["skipped"].append(i)
            continue
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


class Progress:
    """Prints "progress: done/total" on stderr as positions finish, for wrappers."""

    def __init__(self, total: int, done: int = 0):
        self.total, self.done, self.lock = total, done, threading.Lock()
        print(f"progress: {done}/{total}", file=sys.stderr, flush=True)

    def step(self) -> None:
        with self.lock:
            self.done += 1
            print(f"progress: {self.done}/{self.total}", file=sys.stderr, flush=True)


def stream_lines(cmd: list[str], input: str | None = None):
    """Yields cmd's output lines (without newline) as they come; raises if it fails."""
    with subprocess.Popen(
        cmd, stdin=subprocess.PIPE if input is not None else None, stdout=subprocess.PIPE, text=True
    ) as proc:
        if input is not None:

            def feed() -> None:
                proc.stdin.write(input)
                proc.stdin.close()

            threading.Thread(target=feed, daemon=True).start()
        for line in proc.stdout:
            yield line.rstrip("\n")
    if proc.returncode:
        raise subprocess.CalledProcessError(proc.returncode, cmd)


def summarize(keys: list[tuple[int, int]], cgps: dict, log: ResultsLog, limit: float) -> int:
    """Prints how far the run is and the comparison over the finished positions."""
    ours = {(r["job"], r["k"]): r["ours"] for r in log.records if "ours" in r}
    theirs = {(r["job"], r["k"]): r["theirs"] for r in log.records if "theirs" in r}
    finished = [p for p in keys if p in ours and (ours[p].get("skip") or p in theirs)]
    print(f"progress: {len(finished)} of {len(keys)} positions finished")
    stats = compare(
        [(cgps[p], ours[p]) for p in finished], [theirs.get(p, {"skip": True}) for p in finished]
    )
    print(
        f"positions: {stats['positions']}, plays: {stats['plays']},"
        f" mismatched positions: {stats['mismatched']}"
    )
    if stats["skipped"]:
        skipped = [f"{finished[i][0]}.{finished[i][1]}" for i in stats["skipped"]]
        print(f"skipped at the {limit:g} s limit: {len(skipped)} positions ({', '.join(skipped)})")
    for cgp, diffs in stats["examples"]:
        print(f"  {cgp}")
        for d in diffs[:5]:
            print(f"    {d}")
    return 1 if stats["mismatched"] else 0


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
    ap.add_argument(
        "--time-limit",
        type=float,
        default=0,
        help="seconds per position on each side; slower positions are skipped (0 = none)",
    )
    ap.add_argument(
        "--results",
        type=Path,
        help="keep every finished position in this file and skip them when rerun",
    )
    ap.add_argument("--fresh", action="store_true", help="delete --results first")
    ap.add_argument("--status", action="store_true", help="only report on --results")
    ap.add_argument("--macondo-threads", type=int, default=1, help="threads per Macondo process")
    ap.add_argument("--go", default=shutil.which("go") or str(Path.home() / ".local/go/bin/go"))
    args = ap.parse_args()

    if args.fresh and args.results and args.results.exists():
        args.results.unlink()
    log = ResultsLog(args.results, read_only=args.status)
    # The jobs split the positions (job j plays the games of seed + j), so they are
    # part of what the file holds.
    params = {"count": args.count, "max_bag": args.max_bag, "candidates": args.candidates,
              "pass": args.with_pass, "seed": args.seed, "jobs": args.jobs,
              "time_limit": args.time_limit, "lexicon": args.lexicon.name}  # fmt: skip
    saved = [r["params"] for r in log.records if "params" in r]
    if saved and saved[0] != params:
        sys.exit(f"{args.results} was made with {saved[0]}; use --fresh to start over")
    if not saved and not args.status:
        log.append({"params": params})

    apps = args.build / "engine"

    def positions(j: int) -> list[str]:
        cmd = [
            str(apps / "kingbble_positions"),
            "--lexicon", str(args.lexicon),
            "--leaves", str(args.leaves),
            "--name", LEX_NAME,
            "--count", str(args.count),
            "--seed", str(args.seed + j),
            "--preendgame", str(args.max_bag),
            "--candidates", str(args.candidates),
        ]  # fmt: skip
        return cmd + (["--pass"] if args.with_pass else [])

    cgps = {}
    for j in range(args.jobs):
        for k, cgp in enumerate(run(positions(j), capture_output=True).stdout.splitlines()):
            cgps[(j, k)] = cgp
    keys = sorted(cgps)
    if args.status:
        return summarize(keys, cgps, log, args.time_limit)

    work = args.work
    gaddag_dir = work / "data" / "lexica" / "gaddag"
    gaddag_dir.mkdir(parents=True, exist_ok=True)
    shutil.copytree(
        args.macondo / "data" / "letterdistributions",
        work / "data" / "letterdistributions",
        dirs_exist_ok=True,
    )
    run([str(apps / "kingbble_buildkwg"), str(args.lexicon), str(gaddag_dir / f"{LEX_NAME}.kwg")])

    helper_src = args.macondo / "cmd" / "kingbble_preendgame"
    helper_src.mkdir(exist_ok=True)
    shutil.copy(HELPER, helper_src / "main.go")
    helper = work / "kingbble_preendgame"
    run([args.go, "build", "-o", str(helper), "./cmd/kingbble_preendgame"], cwd=args.macondo)

    ours = {(r["job"], r["k"]): r["ours"] for r in log.records if "ours" in r}
    theirs = {(r["job"], r["k"]): r["theirs"] for r in log.records if "theirs" in r}
    # A position counts twice: our solve and Macondo's (none when we skipped it).
    skipped = sum(1 for v in ours.values() if v.get("skip"))
    progress = Progress(2 * len(keys), len(ours) + len(theirs) + skipped)

    def ours_job(j: int) -> None:
        todo = [k for (jj, k) in keys if jj == j and (j, k) not in ours]
        if not todo:
            return
        cmd = positions(j) + ["--solve", "--exhaustive", "--only", ",".join(map(str, todo)),
                              "--time-limit", str(args.time_limit)]  # fmt: skip
        out = stream_lines(cmd)
        for k, _cgp in zip(todo, out):
            sol = json.loads(next(out))
            ours[(j, k)] = sol
            log.append({"job": j, "k": k, "ours": sol})
            progress.step()
            if sol.get("skip"):
                progress.step()  # nothing to ask Macondo

    def macondo_job(j: int) -> None:
        todo = [k for (jj, k) in keys
                if jj == j and not ours[(j, k)].get("skip") and (j, k) not in theirs]  # fmt: skip
        if not todo:
            return
        lines = "".join(
            cgps[(j, k)] + "\t" + ";".join(p["move"] for p in ours[(j, k)]["plays"]) + "\n"
            for k in todo
        )
        cmd = [str(helper), "-data", str(work / "data"), "-lex", LEX_NAME, "-no-cutoff",
               "-threads", str(args.macondo_threads), "-timeout", str(args.time_limit)]  # fmt: skip
        for k, line in zip(todo, stream_lines(cmd, lines)):
            theirs[(j, k)] = json.loads(line)
            log.append({"job": j, "k": k, "theirs": theirs[(j, k)]})
            progress.step()

    with ThreadPoolExecutor(args.jobs) as pool:
        start = time.monotonic()
        list(pool.map(ours_job, range(args.jobs)))
        ours_time = time.monotonic() - start
        start = time.monotonic()
        list(pool.map(macondo_job, range(args.jobs)))
        macondo_time = time.monotonic() - start
    print(f"wall time with {args.jobs} jobs (this run): ours {ours_time:.0f} s,"
          f" macondo {macondo_time:.0f} s")  # fmt: skip
    return summarize(keys, cgps, log, args.time_limit)

if __name__ == "__main__":
    sys.exit(main())
