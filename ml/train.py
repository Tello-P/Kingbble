"""Training at a data level: self-play games, the move model and its learning curve.

  python -m ml.train --level 0 [--games N] [--work build/m6] [--bots equity,equity]
                     [--human-params FILE] [--min-rating R] [--max-rating R]
                     [--movers all] [--seed 1] [--threads 3] [--mem 6G] [--force]

Levels (data scale): 0 smoke = 1,000 self-play games, 1 test = 10,000, both
fitted with the conditional logit (ml.human_model.logit); --games sets any other
size. The games are generated only up to the level's size (kingbble_selfplay
resumes), turned into the turn store and the per-option features, and the
model is fitted on 25, 50 and 100 % of the train turns, each scored on the same
val turns: the learning curve. Every run saves a new version under
<work>/models/vNNN: model.txt (read by HumanModelParams::FromFile), curve.json
and config.json. Every engine and fitting step runs under ml.limits.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path

from ml.limits import Limits, busy

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK = ROOT / "build" / "m6"
DEFAULT_LEXICON = ROOT / "lexica" / "CSW24.txt"
DEFAULT_LEAVES = ROOT / "lexica" / "CSW24.klv2"
SELFPLAY = ROOT / "build" / "engine" / "kingbble_selfplay"
FEATURES = ROOT / "build" / "engine" / "kingbble_features"
LEVELS = {0: ("smoke", 1000), 1: ("test", 10000)}
FRACTIONS = (0.25, 0.5, 1.0)


@dataclass
class Config:
    level: int = 0
    games: int = 0  # 0: the level's size
    lexicon: str = str(DEFAULT_LEXICON)
    leaves: str = str(DEFAULT_LEAVES)
    bots: tuple[str, str] = ("equity", "equity")
    human_params: str = ""  # for a "human" bot
    movers: str = "all"
    min_rating: int | None = None
    max_rating: int | None = None
    seed: int = 1
    max_val: int = 20000
    top_k: str = "64,128"
    # One penalty per fit (as #42's quick mode): the full 3 x 3 grid costs 9 fits.
    l2: str = "1e-2"
    l1: str = "0"
    fractions: tuple[float, ...] = FRACTIONS
    extra: dict = field(default_factory=dict)

    def size(self) -> int:
        return self.games or LEVELS[self.level][1]


def data_dir(work: Path, cfg: Config) -> Path:
    """One directory per way of generating games; larger levels extend it."""
    return work / "data" / f"{cfg.bots[0]}_vs_{cfg.bots[1]}_s{cfg.seed}"


def generate(cfg: Config, data: Path, limits: Limits) -> int:
    """Plays the missing games up to cfg.size(); returns the games in the results file."""
    data.mkdir(parents=True, exist_ok=True)
    cmd = [SELFPLAY, "--lexicon", cfg.lexicon, "--leaves", cfg.leaves, "--a", cfg.bots[0],
           "--b", cfg.bots[1], "--games", cfg.size(), "--seed", cfg.seed,
           "--results", data / "results.txt", "--gcg-dir", data / "gcg"]
    if cfg.human_params:
        cmd += ["--human-params", cfg.human_params]
    limits.run(cmd, stdout=subprocess.DEVNULL)
    return finished_games(data)


def finished_games(data: Path) -> int:
    path = data / "results.txt"
    return sum(1 for line in path.read_text().splitlines() if line.startswith("game ")) \
        if path.exists() else 0


def prepare(cfg: Config, data: Path, limits: Limits) -> Path:
    """Turn store and features of the first cfg.size() games, rebuilt when the size changes."""
    from ml.data.store import build as build_store

    feats = data / f"features_{cfg.size()}"
    if (feats / "turns.tsv").exists():
        return feats
    raw = data / f"raw_{cfg.size()}"
    if raw.exists():
        shutil.rmtree(raw)
    raw.mkdir()
    for g in range(1, cfg.size() + 1):
        src = data / "gcg" / f"game{g}.gcg"
        if src.exists():
            (raw / src.name).symlink_to(src.resolve())
    store = data / f"store_{cfg.size()}"
    build_store(raw, store, salt="selfplay", source="selfplay")
    from ml.data.features import store_turns, write_turn_info

    tmp = feats.with_name(feats.name + ".tmp")
    if tmp.exists():
        shutil.rmtree(tmp)
    write_turn_info(store_turns(store), tmp / "turn_info.tsv")
    limits.run([FEATURES, "--lexicon", cfg.lexicon, "--leaves", cfg.leaves, "--games", raw,
                "--out", tmp, "--top", "128", "--turn-info", tmp / "turn_info.tsv"],
               stdout=subprocess.DEVNULL)
    tmp.rename(feats)
    return feats


def train_turns(feats: Path, store: Path, cfg: Config) -> int:
    """Train turns the fit can use: by split, movers and rating range."""
    from ml.data.features import ratings
    from ml.human_model.logit import in_rating_range

    info = in_rating_range(ratings(feats, store), cfg.min_rating, cfg.max_rating)
    return sum(
        1 for rating, split, bot in info.values()
        if split == "train" and (cfg.movers == "all" or (cfg.movers == "bot") == bot)
    )


def fit(cfg: Config, feats: Path, store: Path, out: Path, n_train: int,
        limits: Limits) -> dict:
    """Fits the logit on n_train turns; its report (val_nll_logit and the rest)."""
    model, report = out.with_suffix(".txt"), out.with_suffix(".json")
    cmd = [sys.executable, "-m", "ml.human_model.logit", "--features", feats, "--store", store,
           "--out", model, "--report", report, "--movers", cfg.movers,
           "--max-train", n_train, "--search-train", min(n_train, 20000),
           "--max-val", cfg.max_val, "--top-k", cfg.top_k, "--l2", cfg.l2, "--l1", cfg.l1]
    if cfg.min_rating is not None:
        cmd += ["--min-rating", cfg.min_rating]
    if cfg.max_rating is not None:
        cmd += ["--max-rating", cfg.max_rating]
    limits.run(cmd, stdout=subprocess.DEVNULL, cwd=ROOT)
    return json.loads(report.read_text())


def next_version(models: Path) -> Path:
    numbers = [int(p.name[1:]) for p in models.glob("v[0-9][0-9][0-9]") if p.is_dir()]
    return models / f"v{max(numbers, default=0) + 1:03d}"


def latest_version(models: Path) -> Path | None:
    done = sorted(p for p in models.glob("v[0-9][0-9][0-9]") if (p / "model.txt").exists())
    return done[-1] if done else None


def still_dropping(curve: list[dict], min_gain: float = 0.002) -> bool:
    """True when doubling the data (50 -> 100 %) still lowers the val NLL by min_gain."""
    by_fraction = {c["fraction"]: c["val_nll"] for c in curve}
    if 0.5 not in by_fraction or 1.0 not in by_fraction:
        return False
    return by_fraction[0.5] - by_fraction[1.0] >= min_gain


def train(cfg: Config, work: Path = DEFAULT_WORK, limits: Limits | None = None,
          log=print) -> Path:
    """Generates, prepares and fits; returns the new version's directory."""
    limits = limits or Limits()
    start = time.time()
    data = data_dir(work, cfg)
    log(f"level {cfg.level}: {cfg.size()} games in {data}")
    games = generate(cfg, data, limits)
    if games < cfg.size():
        raise RuntimeError(f"only {games} of {cfg.size()} games were played")
    feats = prepare(cfg, data, limits)
    store = data / f"store_{cfg.size()}"
    n = train_turns(feats, store, cfg)
    if n == 0:
        raise RuntimeError("no train turns")
    version = next_version(work / "models")
    tmp = version.with_name(version.name + ".tmp")
    if tmp.exists():
        shutil.rmtree(tmp)
    tmp.mkdir(parents=True)
    curve = []
    for fraction in cfg.fractions:
        size = max(1, round(n * fraction))
        report = fit(cfg, feats, store, tmp / f"fit_{round(100 * fraction)}", size, limits)
        curve.append({"fraction": fraction, "train_turns": report["train_turns"],
                      "val_turns": report["val_turns"], "val_nll": report["val_nll_logit"]})
        log(f"  {round(100 * fraction)} %: {report['train_turns']} turns, "
            f"val NLL {report['val_nll_logit']:.4f}")
    shutil.copy(tmp / f"fit_{round(100 * cfg.fractions[-1])}.txt", tmp / "model.txt")
    (tmp / "curve.json").write_text(json.dumps(curve, indent=1) + "\n")
    config = asdict(cfg) | {"games_played": games, "data": str(data), "features": str(feats),
                            "raw": str(data / f"raw_{cfg.size()}"),
                            "store": str(store), "train_turns": n, "commit": _commit(),
                            "seconds": round(time.time() - start, 1),
                            "still_dropping": still_dropping(curve)}
    (tmp / "config.json").write_text(json.dumps(config, indent=1) + "\n")
    tmp.rename(version)
    log(f"saved {version}")
    return version


def _commit() -> str:
    r = subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True,
                       cwd=ROOT)
    return r.stdout.strip()


def config_args(ap: argparse.ArgumentParser) -> None:
    """The options shared by ml.train and the menu."""
    ap.add_argument("--level", type=int, choices=sorted(LEVELS), default=0)
    ap.add_argument("--games", type=int, default=0, help="any number of games instead")
    ap.add_argument("--lexicon", default=str(DEFAULT_LEXICON))
    ap.add_argument("--leaves", default=str(DEFAULT_LEAVES))
    ap.add_argument("--bots", default="equity,equity", help="the two self-play bots")
    ap.add_argument("--human-params", default="", help="model of the 'human' bot")
    ap.add_argument("--movers", default="all", choices=("all", "human", "bot"))
    ap.add_argument("--min-rating", type=int)
    ap.add_argument("--max-rating", type=int)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--work", type=Path, default=DEFAULT_WORK)
    ap.add_argument("--threads", type=int, default=Limits().threads)
    ap.add_argument("--mem", default=Limits().mem)
    ap.add_argument("--force", action="store_true", help="run even if another heavy job runs")


def config_from(args) -> tuple[Config, Limits]:
    bots = tuple(args.bots.split(","))
    if len(bots) != 2:
        raise SystemExit("--bots takes two bots, e.g. equity,human")
    cfg = Config(level=args.level, games=args.games, lexicon=args.lexicon, leaves=args.leaves,
                 bots=bots, human_params=args.human_params, movers=args.movers,
                 min_rating=args.min_rating, max_rating=args.max_rating, seed=args.seed)
    return cfg, Limits(args.threads, args.mem)


def check_busy(force: bool) -> None:
    others = busy()
    if others and not force:
        raise SystemExit("another heavy run is active; wait for it or pass --force:\n  "
                         + "\n  ".join(others))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    config_args(ap)
    args = ap.parse_args()
    check_busy(args.force)
    cfg, limits = config_from(args)
    train(cfg, args.work, limits)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
