"""Tournament configs (configs/m7/*.yaml) as kingbble_selfplay arguments.

  python -m ml.eval.tournament_config list [--dir configs/m7]
  python -m ml.eval.tournament_config args CONFIG [--quick] [--threads N]
  python -m ml.eval.tournament_config games CONFIG [--quick]

An experiment file is merged over common.yaml in its directory (nested keys
merge, the experiment wins); with --quick the "quick" section is merged on top.
"args" prints one argument per line for the script to read with mapfile; the
results file and the manifest tags are added by the caller. Every run is
paired and uses per-game seeds, fixed simulation iterations and, for "+eg"
bots, the endgame's depth limit, so it is reproducible.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

import yaml

COMMON = "common.yaml"
REQUIRED = ("name", "a", "b", "seed", "games", "lexicon", "leaves")


def merge(base: dict, over: dict) -> dict:
    """`over` merged into a copy of `base`, nested dicts key by key."""
    out = dict(base)
    for key, value in over.items():
        if isinstance(value, dict) and isinstance(out.get(key), dict):
            out[key] = merge(out[key], value)
        else:
            out[key] = value
    return out


def load(path: Path, quick: bool = False) -> dict:
    """The experiment at `path` merged over its common.yaml (and quick section)."""
    path = Path(path)
    common_path = path.parent / COMMON
    common = (yaml.safe_load(common_path.read_text()) or {}) if common_path.exists() else {}
    config = merge(common, yaml.safe_load(path.read_text()) or {})
    quick_part = config.pop("quick", None) or {}
    if quick:
        config = merge(config, quick_part)
    missing = [k for k in REQUIRED if k not in config]
    if missing:
        raise ValueError(f"{path}: missing {', '.join(missing)}")
    if int(config["games"]) % 2:
        raise ValueError(f"{path}: games must be even (paired deals)")
    return config


def experiments(directory: Path) -> list[Path]:
    """The experiment files of a config directory, by name."""
    return sorted(p for p in Path(directory).glob("*.yaml") if p.name != COMMON)


def config_hash(config: dict) -> str:
    """A short hash of the merged config, for the run's manifest."""
    text = yaml.safe_dump(config, sort_keys=True)
    return hashlib.sha256(text.encode()).hexdigest()[:16]


def selfplay_args(config: dict, threads: int = 1) -> list[str]:
    """kingbble_selfplay arguments for a merged config (without --results)."""
    sim = config.get("sim", {})
    endgame = config.get("endgame", {})
    inference = config.get("inference", {})
    args = ["--lexicon", str(config["lexicon"]), "--leaves", str(config["leaves"])]
    for key, flag in (
        ("human_params", "--human-params"),
        ("word_freq", "--word-freq"),
        ("alt_lexicon", "--alt-lexicon"),
        ("human_rating", "--human-rating"),
    ):
        if config.get(key) is not None:
            args += [flag, str(config[key])]
    args += ["--a", str(config["a"]), "--b", str(config["b"])]
    args += ["--games", str(config["games"]), "--seed", str(config["seed"])]
    args += ["--paired", "--per-game-seeds", "--threads", str(threads)]
    for key, flag in (
        ("candidates", "--sim-candidates"),
        ("iterations", "--sim-iterations"),
        ("plies", "--sim-plies"),
    ):
        if key in sim:
            args += [flag, str(sim[key])]
    if sim.get("time"):
        raise ValueError("sim.time makes a run depend on the machine's speed; use iterations")
    for key, flag in (
        ("time", "--endgame-time"),
        ("plies", "--endgame-plies"),
        ("threads", "--endgame-threads"),
    ):
        if key in endgame:
            args += [flag, str(endgame[key])]
    for key, flag in (
        ("particles", "--particles"),
        ("exact_limit", "--exact-limit"),
        ("surprise_bits", "--surprise-bits"),
        ("posterior_share", "--posterior-share"),
        ("tau", "--tau"),
    ):
        if key in inference:
            args += [flag, str(inference[key])]
    return args


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="cmd", required=True)
    p_list = sub.add_parser("list", help="experiment files, one per line")
    p_list.add_argument("--dir", default="configs/m7")
    for name in ("args", "games", "hash"):
        p = sub.add_parser(name)
        p.add_argument("config")
        p.add_argument("--quick", action="store_true")
        if name == "args":
            p.add_argument("--threads", type=int, default=1)
    a = parser.parse_args(argv)
    if a.cmd == "list":
        for path in experiments(Path(a.dir)):
            print(path)
        return 0
    config = load(Path(a.config), a.quick)
    if a.cmd == "games":
        print(config["games"])
    elif a.cmd == "hash":
        print(config_hash(config))
    else:
        print("\n".join(selfplay_args(config, a.threads)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
