"""Gate between model versions: rack inference log-loss and a paired match.

  python -m ml.evaluate --model build/m6/models/v002 [--previous build/m6/models/v001]
                        [--games 20] [--max-turns 2000] [--sim-candidates 8]
                        [--sim-iterations 50] [--threads 3] [--mem 6G] [--force]

1. Log-loss of the opponent's true leave on the test split of the model's data
   (kingbble_inference_eval on human games mode): the new model, the uniform bag
   ("none") and, if given, the previous version.
2. A paired match (each deal played twice, seats swapped): two sim bots, each
   tracking the other's rack, the new model against the previous version (or
   against uniform racks when there is none); win % and spread with 95 % CIs
   over the pairs.
3. The verdict: "worse" when the match is a significant loss, "better" when it
   is a significant win or the log-loss improves by --min-gain without a
   significant loss, else "not better". Saved as <model>/evaluation.json.
Every step resumes (kingbble_inference_eval's journals) and runs under ml.limits.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import time
from pathlib import Path

from ml.eval.metrics import summarize
from ml.limits import Limits
from ml.train import ROOT, check_busy

EVAL = ROOT / "build" / "engine" / "kingbble_inference_eval"


def test_turns(config: dict, path: Path) -> int:
    """Writes the test-split turns of the model's data as kingbble_inference_eval --turns."""
    import duckdb

    con = duckdb.connect()
    rel = con.read_parquet(str(Path(config["store"]) / "**" / "*.parquet"), hive_partitioning=True)
    rel.create_view("turns")
    movers = config.get("movers", "all")
    rows = con.execute(
        """SELECT game_id, turn, coalesce(rating, 0), is_bot FROM turns
           WHERE split = 'test' AND rack <> ''
             AND (? = 'all' OR (? = 'bot') = is_bot)
           ORDER BY game_id, turn""",
        [movers, movers],
    ).fetchall()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(f"{g}\t{t}\t{r}\t{int(b)}\n" for g, t, r, b in rows))
    return len(rows)


def records(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return [json.loads(line) for line in path.read_text().splitlines() if line.strip()]


def paired_ci(done: dict[int, tuple[float, float]]) -> dict:
    """Win % and mean spread of side a with 95 % CIs over the complete pairs
    (games 2k and 2k+1 share a deal); done: game -> (score a, score b)."""
    pairs = []
    for g in sorted(done):
        if g % 2 or g + 1 not in done:
            continue
        (a0, b0), (a1, b1) = done[g], done[g + 1]
        win = sum(1.0 if a > b else 0.5 if a == b else 0.0 for a, b in ((a0, b0), (a1, b1))) / 2
        pairs.append((win, ((a0 - b0) + (a1 - b1)) / 2))
    n = len(pairs)
    out = {"pairs": n, "games": 2 * n}
    if n < 2:
        return out | {"win": None, "win_lo": None, "win_hi": None, "spread": None}
    for name, idx in (("win", 0), ("spread", 1)):
        xs = [p[idx] for p in pairs]
        mean = sum(xs) / n
        var = sum((x - mean) ** 2 for x in xs) / (n - 1)
        half = 1.96 * math.sqrt(var / n)
        scale = 100 if name == "win" else 1
        out[name] = scale * mean
        out[f"{name}_lo"] = scale * (mean - half)
        out[f"{name}_hi"] = scale * (mean + half)
    return out


def match_scores(done_file: Path) -> dict[int, tuple[float, float]]:
    """game -> (score a, score b) from a kingbble_inference_eval self-play journal."""
    out = {}
    if done_file.exists():
        for line in done_file.read_text().splitlines():
            parts = line.split("\t")
            if len(parts) >= 4 and parts[0].startswith("game"):
                out[int(parts[0][4:])] = (float(parts[2]), float(parts[3]))
    return out


def verdict(log_loss: dict, match: dict, min_gain: float = 0.01) -> str:
    """"worse", "better" or "not better" (see the module docstring)."""
    if match.get("win_hi") is not None and match["win_hi"] < 50:
        return "worse"
    if match.get("win_lo") is not None and match["win_lo"] > 50:
        return "better"
    base = log_loss.get("previous", log_loss.get("none"))
    if base is not None and log_loss.get("new") is not None and base - log_loss["new"] >= min_gain:
        return "better"
    return "not better"


def evaluate(model: Path, previous: Path | None = None, games: int = 20,
             max_turns: int = 2000, sim_candidates: int = 8, sim_iterations: int = 50,
             seed: int = 1, min_gain: float = 0.01, limits: Limits | None = None,
             log=print) -> dict:
    limits = limits or Limits()
    start = time.time()
    config = json.loads((model / "config.json").read_text())
    out = model / "eval"
    out.mkdir(exist_ok=True)
    lex = ["--lexicon", config["lexicon"], "--leaves", config["leaves"]]
    new_params = model / "model.txt"
    prev_params = previous / "model.txt" if previous else None

    turns = out / "test_turns.tsv"
    n_turns = test_turns(config, turns)
    raw = Path(config["raw"])
    log(f"log-loss on {min(n_turns, max_turns)} of {n_turns} test turns")
    runs = [("oracle_new", ["--models", "none,human", "--human-params", new_params])]
    if prev_params:
        runs.append(("oracle_previous", ["--models", "human", "--human-params", prev_params]))
    for name, args in runs:
        limits.run([EVAL, *lex, "--games", raw, "--turns", turns, "--max-turns", max_turns,
                    "--records", out / f"{name}.jsonl", *args], stdout=subprocess.DEVNULL)
    new = summarize(records(out / "oracle_new.jsonl"))
    log_loss = {"none": new.get("none", {}).get("log_loss"),
                "new": new.get("human", {}).get("log_loss"), "turns": new.get("human", {}).get("n")}
    if prev_params:
        prev = summarize(records(out / "oracle_previous.jsonl"))
        log_loss["previous"] = prev.get("human", {}).get("log_loss")

    log(f"paired match, {games} games: new vs {'previous' if prev_params else 'uniform racks'}")
    match_args = ["--games-played", games, "--paired", "--seed", seed, "--a", "human",
                  "--human-params", new_params, "--sim-candidates", sim_candidates,
                  "--sim-iterations", sim_iterations, "--threads", limits.threads,
                  "--records", out / "match.jsonl"]
    match_args += ["--b", "bot", "--bot-params", prev_params] if prev_params else ["--b", "none"]
    if games > 0:
        limits.run([EVAL, *lex, *match_args], stdout=subprocess.DEVNULL)
    match = paired_ci(match_scores(out / "match.jsonl.done"))
    match["opponent"] = str(previous) if previous else "none"

    result = {"model": str(model), "previous": str(previous) if previous else None,
              "log_loss": log_loss, "beats_uniform": _lt(log_loss["new"], log_loss["none"]),
              "match": match, "min_gain": min_gain,
              "verdict": verdict(log_loss, match, min_gain),
              "seconds": round(time.time() - start, 1)}
    (model / "evaluation.json").write_text(json.dumps(result, indent=1) + "\n")
    log(f"log-loss new {_fmt(log_loss['new'])}, uniform {_fmt(log_loss['none'])}"
        + (f", previous {_fmt(log_loss['previous'])}" if prev_params else "")
        + (f"; win % {match['win']:.1f} ({match['win_lo']:.1f}-{match['win_hi']:.1f})"
           if match.get("win") is not None else f"; {match['games']} match games")
        + f": {result['verdict']}")
    return result


def _lt(a, b) -> bool | None:
    return None if a is None or b is None else a < b


def _fmt(x) -> str:
    return "-" if x is None else f"{x:.4f}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--model", type=Path, required=True)
    ap.add_argument("--previous", type=Path)
    ap.add_argument("--games", type=int, default=20, help="match games (even; 0: none)")
    ap.add_argument("--max-turns", type=int, default=2000)
    ap.add_argument("--sim-candidates", type=int, default=8)
    ap.add_argument("--sim-iterations", type=int, default=50)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--min-gain", type=float, default=0.01)
    ap.add_argument("--threads", type=int, default=Limits().threads)
    ap.add_argument("--mem", default=Limits().mem)
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()
    check_busy(args.force)
    evaluate(args.model, args.previous, args.games, args.max_turns, args.sim_candidates,
             args.sim_iterations, args.seed, args.min_gain, Limits(args.threads, args.mem))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
