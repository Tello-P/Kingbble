#!/usr/bin/env python3
"""Differential test of the endgame solver against Macondo.

Dumps empty-bag positions from greedy self-play with our solver's value for the
player to move, asks Macondo's endgame solver for the same positions and
compares the values (the spread gained from there to the end with best play).

Requires a build of this repo (kingbble_positions, kingbble_buildkwg), Go and a
Macondo checkout:

  python scripts/compare_endgame.py --macondo ~/src/macondo --count 200 --max-tiles 10 --jobs 8

With --results FILE each finished position is kept in FILE: stop the run any
time and rerun the same command to go on (--fresh starts over, --status only
reports).
"""

from __future__ import annotations

import argparse
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

HELPER = ROOT / "scripts" / "macondo_endgame" / "main.go"
# Macondo infers the letter distribution from the lexicon name prefix.
LEX_NAME = "NWL_ENABLE"


def value(text: str) -> int | None:
    """A solved value, or None for a position skipped at the time limit."""
    return None if text == "skip" else int(text)


def read_pairs(lines: list[str]) -> list[tuple[str, int | None]]:
    """'<cgp>\\n<value>' pairs."""
    return [(lines[i], value(lines[i + 1])) for i in range(0, len(lines), 2)]


def compare(ours: list[tuple[str, int | None]], theirs: list[int | None]) -> dict:
    stats = {"positions": 0, "mismatched": 0, "examples": [], "skipped": []}
    for i, ((cgp, a), b) in enumerate(zip(ours, theirs, strict=True)):
        if a is None or b is None:
            stats["skipped"].append(i)
            continue
        stats["positions"] += 1
        if a != b:
            stats["mismatched"] += 1
            if len(stats["examples"]) < 5:
                stats["examples"].append((cgp, a, b))
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


def summarize(cgps: list[str], log: ResultsLog, time_limit: float) -> int:
    """Prints how far the run is and the comparison over the finished positions."""
    ours = {r["i"]: r["ours"] for r in log.records if "ours" in r}
    theirs = {r["i"]: r["theirs"] for r in log.records if "theirs" in r}
    finished = [i for i in range(len(cgps)) if i in ours and (ours[i] is None or i in theirs)]
    print(f"progress: {len(finished)} of {len(cgps)} positions finished")
    stats = compare([(cgps[i], ours[i]) for i in finished], [theirs.get(i) for i in finished])
    print(f"positions: {stats['positions']}, mismatched values: {stats['mismatched']}")
    if stats["skipped"]:
        skipped = [finished[k] for k in stats["skipped"]]
        print(
            f"skipped at the {time_limit:g} s limit: {len(skipped)} positions"
            f" (#{', #'.join(map(str, skipped))})"
        )
    for cgp, a, b in stats["examples"]:
        print(f"  {cgp}\n    ours: {a:+d}, macondo: {b:+d}")
    return 1 if stats["mismatched"] else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--macondo", type=Path, default=Path.home() / "src" / "macondo")
    ap.add_argument("--lexicon", type=Path, default=ROOT / "lexica" / "ENABLE.txt")
    ap.add_argument("--build", type=Path, default=ROOT / "build")
    ap.add_argument("--work", type=Path, default=ROOT / "build" / "macondo_compare")
    ap.add_argument("--count", type=int, default=200)
    ap.add_argument("--max-tiles", type=int, default=10)
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
    ap.add_argument("--go", default=shutil.which("go") or str(Path.home() / ".local/go/bin/go"))
    args = ap.parse_args()

    if args.fresh and args.results and args.results.exists():
        args.results.unlink()
    log = ResultsLog(args.results, read_only=args.status)
    params = {"count": args.count, "max_tiles": args.max_tiles, "seed": args.seed,
              "time_limit": args.time_limit, "lexicon": args.lexicon.name}  # fmt: skip
    saved = [r["params"] for r in log.records if "params" in r]
    if saved and saved[0] != params:
        sys.exit(f"{args.results} was made with {saved[0]}; use --fresh to start over")
    if not saved and not args.status:
        log.append({"params": params})

    apps = args.build / "engine"
    positions = [
        str(apps / "kingbble_positions"),
        "--lexicon", str(args.lexicon),
        "--name", LEX_NAME,
        "--count", str(args.count),
        "--seed", str(args.seed),
        "--endgame",
        "--max-tiles", str(args.max_tiles),
    ]  # fmt: skip
    cgps = run(positions, capture_output=True).stdout.splitlines()
    if args.status:
        return summarize(cgps, log, args.time_limit)

    work = args.work
    gaddag_dir = work / "data" / "lexica" / "gaddag"
    gaddag_dir.mkdir(parents=True, exist_ok=True)
    shutil.copytree(
        args.macondo / "data" / "letterdistributions",
        work / "data" / "letterdistributions",
        dirs_exist_ok=True,
    )
    run([str(apps / "kingbble_buildkwg"), str(args.lexicon), str(gaddag_dir / f"{LEX_NAME}.kwg")])

    helper_src = args.macondo / "cmd" / "kingbble_endgame"
    helper_src.mkdir(exist_ok=True)
    shutil.copy(HELPER, helper_src / "main.go")
    helper = work / "kingbble_endgame"
    run([args.go, "build", "-o", str(helper), "./cmd/kingbble_endgame"], cwd=args.macondo)

    ours = {r["i"]: r["ours"] for r in log.records if "ours" in r}
    theirs = {r["i"]: r["theirs"] for r in log.records if "theirs" in r}
    # A position counts twice: our solve and Macondo's (none when we skipped it).
    progress = Progress(2 * len(cgps), len(ours) + len(theirs) + list(ours.values()).count(None))
    index_of: dict[str, list[int]] = {}
    for i, cgp in enumerate(cgps):
        index_of.setdefault(cgp, []).append(i)

    def ours_job(part: list[int]) -> None:
        if not part:
            return
        out = stream_lines(positions + ["--solve", "--only", ",".join(map(str, part)),
                                        "--time-limit", str(args.time_limit)])  # fmt: skip
        for cgp in out:
            v = value(next(out))
            for i in index_of[cgp]:
                if i in part and i not in ours:
                    ours[i] = v
                    log.append({"i": i, "cgp": cgp, "ours": v})
                    progress.step()
                    if v is None:
                        progress.step()  # nothing to ask Macondo

    def macondo_job(part: list[int]) -> None:
        if not part:
            return
        cmd = [str(helper), "-data", str(work / "data"), "-lex", LEX_NAME,
               "-timeout", str(args.time_limit)]  # fmt: skip
        for i, line in zip(
            part, stream_lines(cmd, "".join(cgps[i] + "\n" for i in part)), strict=False
        ):
            theirs[i] = value(line)
            log.append({"i": i, "theirs": theirs[i]})
            progress.step()

    with ThreadPoolExecutor(args.jobs) as pool:
        start = time.monotonic()
        todo = [i for i in range(len(cgps)) if i not in ours]
        list(pool.map(ours_job, [todo[j :: args.jobs] for j in range(args.jobs)]))
        ours_time = time.monotonic() - start
        start = time.monotonic()
        todo = [i for i in range(len(cgps)) if ours[i] is not None and i not in theirs]
        list(pool.map(macondo_job, [todo[j :: args.jobs] for j in range(args.jobs)]))
        macondo_time = time.monotonic() - start
    print(f"ours: {ours_time:.1f} s, macondo: {macondo_time:.1f} s (this run)")
    return summarize(cgps, log, args.time_limit)


if __name__ == "__main__":
    sys.exit(main())
