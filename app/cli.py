"""Training menu: levels, train, evaluate, improve and the Auto loop.

  python -m app.cli                       # interactive menu
  python -m app.cli train [--level 0|1] [--games N] [--min-rating R] [--max-rating R] ...
  python -m app.cli evaluate [--model vNNN] [--previous vMMM] [--eval-games N] ...
  python -m app.cli improve ...           # next level if the learning curve still drops
  python -m app.cli auto --budget-cpu-hours H --budget-gb G [--k 64] ...
  python -m app.cli status

Train generates the missing self-play games of the level, fits the move model
with its learning curve and saves a new version (ml.train); evaluate gates it
against the previous version (ml.evaluate); both are logged to MLflow
(ml.tracking). The Auto loop repeats train and evaluate from the given level:
with k (the move model's candidate cap, fixed here) it trains, evaluates the
new version against the best so far, keeps it unless it loses, and goes up a
level while the learning curve still drops and the new version is better. It
stops when the CPU or disk budget is spent, when it cannot go further, or when
a new version loses (the best one is kept). Its state is saved after every
step in <work>/auto.json, so the same command resumes it. Every heavy step runs
under ml.limits and refuses to start while another heavy run is active, unless
--force.
"""

from __future__ import annotations

import argparse
import json
import resource
import shutil
import sys
from collections.abc import Callable
from dataclasses import replace
from pathlib import Path

from ml import tracking
from ml.evaluate import evaluate
from ml.limits import Limits
from ml.train import (
    LEVELS,
    Config,
    check_busy,
    config_args,
    config_from,
    latest_version,
    still_dropping,
    train,
)

MAX_LEVEL = max(LEVELS)


def cpu_seconds() -> float:
    """CPU time of this process and its finished children (the engine and fit steps)."""
    total = 0.0
    for who in (resource.RUSAGE_SELF, resource.RUSAGE_CHILDREN):
        r = resource.getrusage(who)
        total += r.ru_utime + r.ru_stime
    return total


def disk_bytes(path: Path) -> int:
    return sum(p.stat().st_size for p in path.rglob("*") if p.is_file() and not p.is_symlink()) \
        if path.exists() else 0


class Auto:
    """The Auto loop; train_fn and evaluate_fn are ml.train.train and ml.evaluate.evaluate
    (replaceable in tests), measured against the budget between steps."""

    def __init__(self, cfg: Config, work: Path, limits: Limits, budget_cpu_hours: float,
                 budget_gb: float, eval_kw: dict | None = None,
                 train_fn: Callable = train, evaluate_fn: Callable = evaluate,
                 cpu_fn: Callable[[], float] = cpu_seconds,
                 disk_fn: Callable[[Path], int] = disk_bytes, log=print,
                 max_level: int = MAX_LEVEL):
        self.cfg, self.work, self.limits = cfg, work, limits
        self.budget_cpu = budget_cpu_hours * 3600
        self.budget_disk = budget_gb * 2**30
        self.eval_kw = eval_kw or {}
        self.train_fn, self.evaluate_fn = train_fn, evaluate_fn
        self.cpu_fn, self.disk_fn, self.log = cpu_fn, disk_fn, log
        self.max_level = max_level
        self.path = work / "auto.json"
        self.state = self._load()

    def _load(self) -> dict:
        if self.path.exists():
            return json.loads(self.path.read_text())
        return {"level": self.cfg.level, "k": self.cfg.top_k, "best": None, "pending": None,
                "cpu_seconds": 0.0, "history": [], "stopped": None}

    def _save(self) -> None:
        self.work.mkdir(parents=True, exist_ok=True)
        self.path.write_text(json.dumps(self.state, indent=1) + "\n")

    def _spent(self, start_cpu: float) -> float:
        return self.state["cpu_seconds"] + self.cpu_fn() - start_cpu

    def _over_budget(self, start_cpu: float) -> str | None:
        if self._spent(start_cpu) >= self.budget_cpu:
            return "CPU budget spent"
        if self.disk_fn(self.work) >= self.budget_disk:
            return "disk budget spent"
        return None

    def _stop(self, reason: str, start_cpu: float) -> dict:
        self.state["stopped"] = reason
        self.state["cpu_seconds"] = self._spent(start_cpu)
        self._save()
        self.log(f"auto: stopped ({reason}); best version: {self.state['best']}")
        if self.state["best"]:
            tracking.tag_best(Path(self.state["best"]))
        return self.state

    def run(self) -> dict:
        start_cpu = self.cpu_fn()
        if self.state.get("stopped"):
            self.log(f"auto: already stopped ({self.state['stopped']}); remove {self.path} "
                     "to start over")
            return self.state
        with tracking.run("auto", {"step": "auto"}):
            while True:
                reason = self._over_budget(start_cpu)
                if reason:
                    return self._stop(reason, start_cpu)
                level = self.state["level"]
                if self.state["pending"] is None:
                    # A custom size (e.g. a quick run) grows tenfold per level, as the levels do.
                    games = self.cfg.games * 10 ** (level - self.cfg.level) if self.cfg.games else 0
                    cfg = replace(self.cfg, level=level, games=games, top_k=self.state["k"])
                    self.log(f"auto: level {level} ({LEVELS[level][0]}), k {self.state['k']}")
                    version = self.train_fn(cfg, self.work, self.limits, log=self.log)
                    self.state["pending"] = str(version)
                    self.state["cpu_seconds"] = self._spent(start_cpu)
                    start_cpu = self.cpu_fn()
                    self._save()
                    reason = self._over_budget(start_cpu)
                    if reason:
                        return self._stop(reason, start_cpu)
                version = Path(self.state["pending"])
                best = Path(self.state["best"]) if self.state["best"] else None
                result = self.evaluate_fn(version, best, limits=self.limits, log=self.log,
                                          **self.eval_kw)
                curve = json.loads((version / "curve.json").read_text())
                dropping = still_dropping(curve)
                self.state["history"].append({"level": level, "version": str(version),
                                              "verdict": result["verdict"],
                                              "still_dropping": dropping})
                self.state["pending"] = None
                self.state["cpu_seconds"] = self._spent(start_cpu)
                start_cpu = self.cpu_fn()
                if result["verdict"] == "worse":
                    self._save()
                    return self._stop(f"{version.name} loses to the best version", start_cpu)
                if best is None or result["verdict"] == "better":
                    self.state["best"] = str(version)
                self._save()
                if result["verdict"] != "better" and best is not None:
                    return self._stop(f"{version.name} is not better", start_cpu)
                if not dropping:
                    return self._stop("the learning curve no longer drops", start_cpu)
                if level >= self.max_level:
                    return self._stop(f"level {level} is the last one", start_cpu)
                self.state["level"] = level + 1
                self._save()


def eval_args(ap: argparse.ArgumentParser) -> None:
    ap.add_argument("--eval-games", type=int, default=20, help="paired match games")
    ap.add_argument("--eval-max-turns", type=int, default=2000)
    ap.add_argument("--eval-sim-candidates", type=int, default=8)
    ap.add_argument("--eval-sim-iterations", type=int, default=50)
    ap.add_argument("--min-gain", type=float, default=0.01)


def eval_kw(args) -> dict:
    return {"games": args.eval_games, "max_turns": args.eval_max_turns,
            "sim_candidates": args.eval_sim_candidates,
            "sim_iterations": args.eval_sim_iterations, "min_gain": args.min_gain}


def versions(work: Path) -> list[Path]:
    return sorted(p for p in (work / "models").glob("v[0-9][0-9][0-9]")
                  if (p / "model.txt").exists())


def cmd_train(args) -> int:
    cfg, limits = config_from(args)
    cfg = replace(cfg, top_k=args.k)
    train(cfg, args.work, limits)
    return 0


def cmd_evaluate(args) -> int:
    _, limits = config_from(args)
    done = versions(args.work)
    model = args.model or (done[-1] if done else None)
    if model is None:
        raise SystemExit("no trained version yet: run train first")
    previous = args.previous
    if previous is None and model in done and done.index(model) > 0:
        previous = done[done.index(model) - 1]
    evaluate(model, previous, limits=limits, **eval_kw(args))
    return 0


def cmd_improve(args) -> int:
    latest = latest_version(args.work / "models")
    if latest is None:
        raise SystemExit("no trained version yet: run train first")
    config = json.loads((latest / "config.json").read_text())
    curve = json.loads((latest / "curve.json").read_text())
    level = config["level"]
    if not still_dropping(curve):
        print(f"{latest.name}: the learning curve no longer drops; staying at level {level}")
        return 0
    if level >= MAX_LEVEL:
        print(f"{latest.name}: level {level} is the last one in this menu")
        return 0
    args.level, args.games = level + 1, 0
    return cmd_train(args)


def cmd_auto(args) -> int:
    cfg, limits = config_from(args)
    cfg = replace(cfg, top_k=args.k)
    Auto(cfg, args.work, limits, args.budget_cpu_hours, args.budget_gb, eval_kw(args),
         max_level=args.max_level).run()
    return 0


def cmd_status(args) -> int:
    for v in versions(args.work):
        config = json.loads((v / "config.json").read_text())
        curve = json.loads((v / "curve.json").read_text())
        line = (f"{v.name}: level {config['level']}, {config['games_played']} games, "
                f"val NLL {curve[-1]['val_nll']:.4f}")
        if (v / "evaluation.json").exists():
            e = json.loads((v / "evaluation.json").read_text())
            line += f", {e['verdict']}"
        print(line)
    auto = args.work / "auto.json"
    if auto.exists():
        state = json.loads(auto.read_text())
        print(f"auto: level {state['level']}, best {state['best']}, "
              f"{state['cpu_seconds'] / 3600:.2f} CPU hours, stopped: {state['stopped']}")
    print(f"disk: {disk_bytes(args.work) / 2**30:.2f} GB in {args.work}")
    return 0


def parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="command")
    for name in ("train", "evaluate", "improve", "auto", "status"):
        p = sub.add_parser(name)
        config_args(p)
        p.add_argument("--k", default="64", help="the move model's candidate cap (top_k)")
        eval_args(p)
        if name == "evaluate":
            p.add_argument("--model", type=Path)
            p.add_argument("--previous", type=Path)
        if name == "auto":
            p.add_argument("--budget-cpu-hours", type=float, required=True)
            p.add_argument("--budget-gb", type=float, required=True)
            p.add_argument("--max-level", type=int, choices=sorted(LEVELS), default=MAX_LEVEL)
    return ap


COMMANDS = {"train": cmd_train, "evaluate": cmd_evaluate, "improve": cmd_improve,
            "auto": cmd_auto, "status": cmd_status}


def menu() -> list[str]:
    """Asks for the command and its main options; returns the arguments."""
    def ask(prompt: str, default: str) -> str:
        answer = input(f"{prompt} [{default}]: ").strip()
        return answer or default

    print("1 train  2 evaluate  3 improve  4 auto  5 status")
    command = {"1": "train", "2": "evaluate", "3": "improve", "4": "auto",
               "5": "status"}.get(ask("option", "5"), "status")
    argv = [command]
    if command in ("train", "auto"):
        level = ask("level: 0 smoke (1,000 games), 1 test (10,000), or a number of games", "0")
        argv += ["--level", level] if level in ("0", "1") else ["--games", level]
        low, high = ask("min rating (empty: any)", ""), ask("max rating (empty: any)", "")
        argv += (["--min-rating", low] if low else []) + (["--max-rating", high] if high else [])
    if command == "auto":
        argv += ["--budget-cpu-hours", ask("CPU hours", "1"), "--budget-gb", ask("disk GB", "2")]
    return argv


def main(argv: list[str] | None = None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if not argv and sys.stdin.isatty():
        argv = menu()
    args = parser().parse_args(argv)
    if args.command is None:
        parser().print_help()
        return 2
    if args.command not in ("status",):
        check_busy(args.force)
        for tool in ("kingbble_selfplay", "kingbble_features", "kingbble_inference_eval"):
            if not (Path(__file__).resolve().parents[1] / "build" / "engine" / tool).exists() \
                    and not shutil.which(tool):
                raise SystemExit(f"build the engine first (missing build/engine/{tool})")
    return COMMANDS[args.command](args)


if __name__ == "__main__":
    raise SystemExit(main())
