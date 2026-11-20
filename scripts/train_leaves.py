#!/usr/bin/env python3
"""Trains our own leave table for a lexicon with wolges, then measures it.

    scripts/train_leaves.sh LEXICON_FILE LEVEL [--default-leaves KLV] [--out KLV]

LEVEL is fast (wolges' census alone, minutes), normal (census, one self-play
generation and a blend, about an hour and a half on 2 threads) or max (census,
two long self-play generations and a blend, a night or more). The table goes to
--out (default: LEX.own.klv2 beside the lexicon); the published table beside the
lexicon (LEX.klv2) is never written. When there is one (or --default-leaves), the
new table plays paired equity games against it and the verdict is printed.

Every step resumes from build/leaves/LEX/LEVEL: stop the script and run it again
to go on (once a run has finished, the next one starts afresh).
Threads: KINGBBLE_MAX_THREADS (default 2). Wolges: KINGBBLE_WOLGES (default ~/src/wolges);
its binaries are built when missing. English tiles only.

Lines for the server on stdout, the rest on stderr:
    @progress PCT ETA_SECONDS|- MESSAGE
    @table PATH
    @result WIN_PCT CI_LO CI_HI GAMES better|equal|worse
    @done
"""

from __future__ import annotations

import argparse
import math
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Census boards (wolges' recipe), self-play games per generation, paired games.
LEVELS = {
    "fast": {"recipe": "3x128,512", "gens": [], "eval": 2000},
    "normal": {"recipe": "3x256,2048", "gens": [4_000_000], "eval": 4000},
    "max": {"recipe": "3x256,2048", "gens": [30_000_000, 30_000_000], "eval": 10000},
}
CHUNK = 1_000_000  # self-play games per resumable chunk
# Rough seconds per unit on one thread, only to weigh the steps in the progress.
COST = {"board": 0.8, "game": 0.0018, "eval": 0.05, "build": 30.0}


def recipe_boards(recipe: str) -> int:
    total = 0
    for part in recipe.split(","):
        k, _, n = part.partition("x")
        total += int(k) * int(n) if n else int(k)
    return total


class Progress:
    """Turns step-local progress into one percentage and an ETA."""

    def __init__(self, steps: list[tuple[str, float]], threads: int):
        self.weights = dict(steps)
        self.order = [name for name, _ in steps]
        self.total = sum(self.weights.values()) or 1.0
        self.threads = threads
        self.start = time.monotonic()
        self.start_pct: float | None = None
        self.last = 0.0

    def report(self, step: str, frac: float, message: str) -> None:
        done = sum(self.weights[s] for s in self.order[: self.order.index(step)])
        pct = 100.0 * (done + self.weights[step] * min(max(frac, 0.0), 1.0)) / self.total
        if self.start_pct is None:
            self.start_pct = pct
        now = time.monotonic()
        gained = pct - self.start_pct
        eta = "-"
        if gained > 0.5 and now - self.start > 5:
            eta = str(int((now - self.start) * (100.0 - pct) / gained))
        if now - self.last >= 1 or frac >= 1:
            self.last = now
            print(f"@progress {pct:.1f} {eta} {message}", flush=True)


def run_streaming(argv: list[str], cwd: Path, on_line) -> None:
    proc = subprocess.Popen(
        argv, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1
    )
    assert proc.stdout
    for line in proc.stdout:
        on_line(line.rstrip("\n"))
    if proc.wait() != 0:
        raise RuntimeError(f"{Path(argv[0]).name} {argv[1] if len(argv) > 1 else ''} failed")


def run(argv: list[str], cwd: Path | None = None) -> None:
    res = subprocess.run(argv, cwd=cwd, stdout=sys.stderr, stderr=sys.stderr)
    if res.returncode != 0:
        raise RuntimeError(f"{Path(argv[0]).name} failed")


def wolges_bins(wolges: Path) -> tuple[Path, Path]:
    leave, buildlex = wolges / "target/release/leave", wolges / "target/release/buildlex"
    if not (leave.is_file() and buildlex.is_file()):
        if not (wolges / "Cargo.toml").is_file():
            raise RuntimeError(f"no wolges checkout in {wolges} (KINGBBLE_WOLGES)")
        run(["cargo", "build", "--release", "--bin", "leave", "--bin", "buildlex"], cwd=wolges)
    return leave, buildlex


def paired_ci(results: Path, games: int) -> tuple[float, float, float, int]:
    """Win % of bot a with its 95 % CI over finished pairs (games 2k-1 and 2k share a deal)."""
    score: dict[int, float] = {}
    pat = re.compile(r"^game (\d+): (-?\d+) - (-?\d+), a in seat ([12])")
    for line in results.read_text().splitlines():
        m = pat.match(line)
        if not m or int(m[1]) > games:
            continue
        a, b = (int(m[2]), int(m[3])) if m[4] == "1" else (int(m[3]), int(m[2]))
        score[int(m[1])] = 1.0 if a > b else 0.0 if a < b else 0.5
    pairs = [(score[g] + score[g + 1]) / 2 for g in score if g % 2 == 1 and g + 1 in score]
    n = len(pairs)
    if n < 2:
        return 50.0, 0.0, 100.0, 2 * n
    mean = sum(pairs) / n
    var = sum((p - mean) ** 2 for p in pairs) / (n - 1) / n
    w = 1.96 * math.sqrt(var)
    return 100 * mean, 100 * (mean - w), 100 * (mean + w), 2 * n


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("lexicon", type=Path, help="word list (.txt or .kwg)")
    ap.add_argument("level", choices=sorted(LEVELS))
    ap.add_argument(
        "--default-leaves", type=Path, help="table to measure against (default: LEX.klv2)"
    )
    ap.add_argument("--out", type=Path, help="where the table goes (default: LEX.own.klv2)")
    a = ap.parse_args()

    lexicon = a.lexicon.resolve()
    if not lexicon.is_file():
        print(f"no such lexicon: {lexicon}", file=sys.stderr)
        return 2
    name = lexicon.stem
    out = (a.out or lexicon.with_name(f"{name}.own.klv2")).resolve()
    default = a.default_leaves or lexicon.with_suffix(".klv2")
    default = default.resolve() if default.is_file() else None
    if out == default:
        print("--out would overwrite the published table", file=sys.stderr)
        return 2
    level = LEVELS[a.level]
    threads = max(1, int(os.environ.get("KINGBBLE_MAX_THREADS", "2")))
    wolges = Path(os.environ.get("KINGBBLE_WOLGES", Path.home() / "src/wolges"))
    apps = ROOT / "build/engine"
    work = ROOT / "build/leaves" / name / a.level
    if (work / "done").is_file():  # a finished run is never resumed: train afresh
        shutil.rmtree(work)
    work.mkdir(parents=True, exist_ok=True)

    boards = recipe_boards(level["recipe"])
    steps = [("prepare", COST["build"]), ("census", boards * COST["board"])]
    for i, games in enumerate(level["gens"], 1):
        steps.append((f"gen{i}", games * COST["game"] + COST["build"]))
    if default:
        steps.append(("eval", level["eval"] * COST["eval"]))
    prog = Progress(steps, threads)

    try:
        leave, buildlex = wolges_bins(wolges)
        prog.report("prepare", 0, "preparing")
        kwg = lexicon if lexicon.suffix == ".kwg" else lexicon.with_suffix(".kwg")
        if not kwg.is_file():
            kwg = work / f"{name}.kwg"
            if not kwg.is_file():
                run([str(apps / "kingbble_buildkwg"), str(lexicon), str(kwg) + ".tmp"])
                os.replace(str(kwg) + ".tmp", kwg)
        prog.report("prepare", 1, "word graph ready")

        threads_opt = ["--threads", str(threads)]
        census = work / "census.klv2"
        if not census.is_file():
            tmp = work / "census_tmp"
            shutil.rmtree(tmp, ignore_errors=True)
            tmp.mkdir()
            board_re = re.compile(r"census: board (\d+)/(\d+)")
            stage = {"base": 0}

            def on_census(line: str) -> None:
                m = board_re.search(line)
                if not m:
                    return
                k, n = int(m[1]), int(m[2])
                if k == n:
                    stage["base"] += n
                prog.report(
                    "census",
                    (stage["base"] + (k if k < n else 0)) / boards,
                    f"census: board {min(stage['base'] + (k if k < n else 0), boards)}/{boards}",
                )

            run_streaming(
                [str(leave), *threads_opt, "census", str(kwg), "-", "-", level["recipe"]],
                tmp,
                on_census,
            )
            made = sorted(tmp.glob("census-leaves-*.klv2"), key=lambda p: p.stat().st_mtime)
            if not made:
                raise RuntimeError("the census wrote no leave table")
            os.replace(made[-1], census)
            shutil.rmtree(tmp, ignore_errors=True)
        prog.report("census", 1, "census done")

        last = census
        for i, games in enumerate(level["gens"], 1):
            step, gen_dir, table = f"gen{i}", work / f"gen{i}", work / f"gen{i}.klv2"
            if table.is_file():
                last = table
                prog.report(step, 1, f"self-play generation {i} done")
                continue
            gen_dir.mkdir(exist_ok=True)
            chunks = math.ceil(games / CHUNK)
            for k in range(chunks):
                chunk = gen_dir / f"chunk{k}.csv"
                size = min(CHUNK, games - k * CHUNK)
                if chunk.is_file():
                    continue
                tmp = gen_dir / "tmp"
                shutil.rmtree(tmp, ignore_errors=True)
                tmp.mkdir()
                logged_re = re.compile(r"have logged (\d+) games")

                def on_play(
                    line: str, k=k, i=i, step=step, games=games, logged_re=logged_re
                ) -> None:
                    m = logged_re.search(line)
                    if m:
                        played = k * CHUNK + int(m[1])
                        prog.report(
                            step,
                            0.98 * played / games,
                            f"self-play generation {i}: {played:,} of {games:,} games",
                        )

                run_streaming(
                    [
                        str(leave),
                        *threads_opt,
                        "autoplay-summarize-only",
                        str(kwg),
                        str(last),
                        str(last),
                        str(size),
                        "0",
                    ],
                    tmp,
                    on_play,
                )
                summaries = list(tmp.glob("summary-log-*"))
                if not summaries:
                    raise RuntimeError("self-play wrote no summary")
                os.replace(summaries[0], chunk)
                shutil.rmtree(tmp, ignore_errors=True)
            prog.report(step, 0.98, f"self-play generation {i}: building the table")
            joined = gen_dir / "joined.csv"
            with joined.open("w") as dst:
                for k in range(chunks):
                    dst.write((gen_dir / f"chunk{k}.csv").read_text())
            run([str(leave), "resummarize", "--quiet", str(joined), str(gen_dir / "summary.csv")])
            run(
                [
                    str(leave),
                    *threads_opt,
                    "generate",
                    str(gen_dir / "summary.csv"),
                    str(gen_dir / "leaves.csv"),
                ]
            )
            run([str(buildlex), "klv", str(gen_dir / "leaves.csv"), str(table) + ".tmp"])
            os.replace(str(table) + ".tmp", table)
            shutil.rmtree(gen_dir, ignore_errors=True)
            last = table
            prog.report(step, 1, f"self-play generation {i} done")

        final = work / "final.klv2"
        if last != census:
            run([str(buildlex), "blend", str(census), str(last), "0.5", str(final) + ".tmp"])
            os.replace(str(final) + ".tmp", final)
        else:
            shutil.copyfile(census, final)
        shutil.copyfile(final, str(out) + ".tmp")
        os.replace(str(out) + ".tmp", out)
        print(f"@table {out}", flush=True)

        if default:
            results = work / "eval.txt"
            games = level["eval"]
            proc = subprocess.Popen(
                [
                    str(apps / "kingbble_selfplay"),
                    "--lexicon",
                    str(lexicon),
                    "--leaves",
                    str(default),
                    "--a-leaves",
                    str(final),
                    "--a",
                    "equity",
                    "--b",
                    "equity",
                    "--paired",
                    "--games",
                    str(games),
                    "--seed",
                    "22",
                    "--threads",
                    str(threads),
                    "--results",
                    str(results),
                ],
                stdout=sys.stderr,
                stderr=sys.stderr,
            )
            while proc.poll() is None:
                time.sleep(1)
                played = len(results.read_text().splitlines()) if results.is_file() else 0
                prog.report("eval", played / games, f"measuring: {played:,} of {games:,} games")
            if proc.returncode != 0:
                raise RuntimeError("the paired games failed")
            win, lo, hi, n = paired_ci(results, games)
            verdict = "better" if lo > 50 else "worse" if hi < 50 else "equal"
            prog.report("eval", 1, "measured")
            print(f"@result {win:.1f} {lo:.1f} {hi:.1f} {n} {verdict}", flush=True)
        (work / "done").touch()
        print("@done", flush=True)
        return 0
    except RuntimeError as e:
        print(f"error: {e}", file=sys.stderr)
        print(f"@error {e}", flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
