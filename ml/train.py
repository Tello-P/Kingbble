"""Training at a data level: the move model and its learning curve.

  python -m ml.train --level 0 [--games N] [--source selfplay|human] [--work build/m6]
                     [--bots equity,equity] [--human-params FILE] [--min-rating R]
                     [--max-rating R] [--movers all] [--mlp] [--drop A] [--top-k 64,128]
                     [--l2 1e-2] [--max-train 60000] [--fractions 0.25,0.5,1]
                     [--seed 1] [--threads 3] [--mem 6G] [--force]

Levels (data scale): 0 smoke = 1,000 games, 1 test = 10,000 (the conditional
logit, ml.human_model.logit), 2 base = 100,000 (and a small network), 3 full =
1,000,000 (and a bigger network); --games sets any other size. Every run logs
its rough time and disk first (--estimate prints them and exits).
--source selfplay (the default) generates self-play games up to the level's
size (kingbble_selfplay resumes) and builds their turn store: the pipeline
smoke and the bot model, never the human model (a bot is trivially predictable
and the "human" bot only re-learns itself). --source human takes the first
games of the downloaded human games on the lexicon (the turn store, split by
player) and fits human movers only, with the word familiarity table counted on
their train split. The model is fitted on the --fractions of the train turns
(at most --max-train), each scored on the same val turns: the learning curve;
the largest fit also scores the test split against a uniform choice among the
legal options and, with --mlp, fits the network (kept if better on val). Every
run saves a new version under <work>/models/vNNN: model.txt (read by
HumanModelParams::FromFile), curve.json and config.json. Every engine and
fitting step runs under ml.limits.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field, replace
from pathlib import Path

from ml import tracking
from ml.limits import Limits, busy

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK = ROOT / "build" / "m6"
DEFAULT_LEXICON = ROOT / "lexica" / "CSW24.txt"
DEFAULT_LEAVES = ROOT / "lexica" / "CSW24.klv2"
SELFPLAY = ROOT / "build" / "engine" / "kingbble_selfplay"
FEATURES = ROOT / "build" / "engine" / "kingbble_features"
LEVELS = {0: ("smoke", 1000), 1: ("test", 10000), 2: ("base", 100000), 3: ("full", 1000000)}
# Per level: train turns of the largest fit and the network (hidden units, 0 = logit
# only). Levels 2 and 3 always fit the network; the RL step of level 3 is #53's.
LEVEL_MODEL = {0: (60000, 0), 1: (60000, 0), 2: (300000, 32), 3: (1000000, 64)}
# Rough costs on 2 cores, measured on the M6a and #67 runs: equity self-play games
# per hour, feature extraction seconds per game, fit seconds per 1,000 train turns
# (logit, network), feature bytes per game (self-play, human).
GAMES_PER_HOUR = 25000
FEATURE_SECONDS_PER_GAME = 0.07
FIT_SECONDS_PER_1000 = (15.0, 15.0)
FEATURE_BYTES_PER_GAME = {"selfplay": 1.0e6, "human": 2.2e6}
TURNS_PER_GAME = 22
FRACTIONS = (0.25, 0.5, 1.0)
HUMAN_RAW = ROOT / "data" / "raw" / "woogles"
HUMAN_STORE = ROOT / "data" / "turns"
ALT_LEXICON = ROOT / "lexica" / "ENABLE.txt"


@dataclass
class Config:
    level: int = 0
    games: int = 0  # 0: the level's size
    # "selfplay": games played here (the pipeline smoke, the bot model);
    # "human": downloaded human games (the human move model, human movers only).
    source: str = "selfplay"
    human_raw: str = str(HUMAN_RAW)
    human_store: str = str(HUMAN_STORE)
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
    max_train: int = 0  # turns of the largest fit (0: the level's); the fractions are of this
    mlp: bool = False  # also fit the network on the largest fit, kept if better on val
    mlp_hidden: int = 0  # the network's hidden units (0: the level's, else 32)
    drop: str = ""  # feature groups to leave out, e.g. "A"
    fractions: tuple[float, ...] = FRACTIONS
    extra: dict = field(default_factory=dict)

    def size(self) -> int:
        return self.games or LEVELS[self.level][1]

    def with_level_model(self) -> Config:
        """The level's largest fit and network where they are left to the level."""
        turns, hidden = LEVEL_MODEL.get(self.level, LEVEL_MODEL[max(LEVEL_MODEL)])
        return replace(
            self,
            max_train=self.max_train or turns,
            mlp=self.mlp or hidden > 0,
            mlp_hidden=self.mlp_hidden or hidden or 32,
        )


def estimate(cfg: Config) -> dict:
    """Rough wall hours (on 2 cores) and disk GB of a level's run, before it starts."""
    cfg = cfg.with_level_model()
    games = cfg.size()
    hours = games * FEATURE_SECONDS_PER_GAME / 3600
    if cfg.source == "selfplay":
        hours += games / GAMES_PER_HOUR
    turns = min(games * TURNS_PER_GAME, cfg.max_train)
    fit_s = sum(f * turns / 1000 for f in cfg.fractions) * FIT_SECONDS_PER_1000[0]
    if cfg.mlp:
        fit_s += turns / 1000 * FIT_SECONDS_PER_1000[1] * cfg.mlp_hidden / 32
    hours += fit_s / 3600
    gb = games * FEATURE_BYTES_PER_GAME.get(cfg.source, 1.0e6) / 2**30
    return {"games": games, "hours": round(hours, 1), "gb": round(gb, 1)}


def data_dir(work: Path, cfg: Config) -> Path:
    """One directory per way of getting games; larger levels extend it."""
    if cfg.source == "human":
        return work / "data" / f"human_{Path(cfg.lexicon).stem}"
    return work / "data" / f"{cfg.bots[0]}_vs_{cfg.bots[1]}_s{cfg.seed}"


def human_games(cfg: Config) -> list[str]:
    """Ids of the downloaded human games on cfg's lexicon (the turn store's view), sorted."""
    import duckdb

    con = duckdb.connect()
    pattern = str(Path(cfg.human_store) / "**" / "*.parquet")
    con.read_parquet(pattern, hive_partitioning=True).create_view("turns")
    rows = con.execute(
        "SELECT DISTINCT game_id FROM turns WHERE lexicon = ? ORDER BY game_id",
        [Path(cfg.lexicon).stem],
    ).fetchall()
    return [g for (g,) in rows if (Path(cfg.human_raw) / f"{g}.gcg").exists()]


def prepare_human(cfg: Config, data: Path, limits: Limits) -> tuple[Path, Path, int]:
    """Features of the first cfg.size() human games (all of them if there are fewer), with
    the word familiarity table counted on their train split; (features, raw, games)."""
    from ml.data.features import store_turns, write_turn_info
    from ml.data.word_freq import play_counts, read_words, table, write

    ids = human_games(cfg)[: cfg.size()]
    feats, raw = data / f"features_{len(ids)}", data / f"raw_{len(ids)}"
    if (feats / "turns.tsv").exists():
        return feats, raw, len(ids)
    if raw.exists():
        shutil.rmtree(raw)
    raw.mkdir(parents=True)
    for g in ids:
        (raw / f"{g}.gcg").symlink_to((Path(cfg.human_raw) / f"{g}.gcg").resolve())
    store = Path(cfg.human_store)
    word_freq = data / f"word_freq_{len(ids)}.tsv"
    try:
        from wordfreq import zipf_frequency

        def zipf(w):
            return zipf_frequency(w.lower(), "en")
    except ImportError:  # the "freq" extra: without it only the play counts

        def zipf(w):
            return 0.0

    counts = play_counts(Path(cfg.lexicon), raw, store, FEATURES)
    write(table(read_words(Path(cfg.lexicon)), counts, zipf), word_freq)
    tmp = feats.with_name(feats.name + ".tmp")
    if tmp.exists():
        shutil.rmtree(tmp)
    write_turn_info(store_turns(store), tmp / "turn_info.tsv")
    cmd = [FEATURES, "--lexicon", cfg.lexicon, "--leaves", cfg.leaves, "--games", raw,
           "--out", tmp, "--top", "256", "--turn-info", tmp / "turn_info.tsv",
           "--word-freq", word_freq]  # fmt: skip
    if ALT_LEXICON.exists():
        cmd += ["--alt-lexicon", ALT_LEXICON]
    limits.run(cmd, stdout=subprocess.DEVNULL)
    tmp.rename(feats)
    return feats, raw, len(ids)


def uniform_test_nll(feats: Path, store: Path, cfg: Config) -> float | None:
    """NLL of a uniform choice among all legal options on the fit's test turns."""
    import math

    from ml.data.features import load, ratings
    from ml.human_model.logit import in_rating_range

    info = in_rating_range(ratings(feats, store), cfg.min_rating, cfg.max_rating)
    turns, _ = load(feats)
    logs = []
    for t in turns:
        rating, split, bot = info.get((t.game_id, t.turn), (None, "", False))
        if split == "test" and _mover_ok(cfg.movers, bot):
            logs.append(math.log(t.options))
    return sum(logs) / len(logs) if logs else None


def _mover_ok(movers: str, bot: bool) -> bool:
    return movers == "all" or (movers == "bot") == bot


def generate(cfg: Config, data: Path, limits: Limits) -> int:
    """Plays the missing games up to cfg.size(); returns the games in the results file."""
    data.mkdir(parents=True, exist_ok=True)
    cmd = [
        SELFPLAY,
        "--lexicon",
        cfg.lexicon,
        "--leaves",
        cfg.leaves,
        "--a",
        cfg.bots[0],
        "--b",
        cfg.bots[1],
        "--games",
        cfg.size(),
        "--seed",
        cfg.seed,
        "--results",
        data / "results.txt",
        "--gcg-dir",
        data / "gcg",
    ]
    if cfg.human_params:
        cmd += ["--human-params", cfg.human_params]
    limits.run(cmd, stdout=subprocess.DEVNULL)
    return finished_games(data)


def finished_games(data: Path) -> int:
    path = data / "results.txt"
    return (
        sum(1 for line in path.read_text().splitlines() if line.startswith("game "))
        if path.exists()
        else 0
    )


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
    limits.run(
        [
            FEATURES,
            "--lexicon",
            cfg.lexicon,
            "--leaves",
            cfg.leaves,
            "--games",
            raw,
            "--out",
            tmp,
            "--top",
            "128",
            "--turn-info",
            tmp / "turn_info.tsv",
        ],
        stdout=subprocess.DEVNULL,
    )
    tmp.rename(feats)
    return feats


def train_turns(feats: Path, store: Path, cfg: Config) -> int:
    """Train turns the fit can use: by split, movers and rating range."""
    from ml.data.features import ratings
    from ml.human_model.logit import in_rating_range

    info = in_rating_range(ratings(feats, store), cfg.min_rating, cfg.max_rating)
    return sum(
        1 for rating, split, bot in info.values() if split == "train" and _mover_ok(cfg.movers, bot)
    )


def fit(
    cfg: Config,
    feats: Path,
    store: Path,
    out: Path,
    n_train: int,
    limits: Limits,
    final: bool = False,
) -> dict:
    """Fits the logit on n_train turns; its report (val_nll_logit and the rest). The final
    fit also scores the test split and, with cfg.mlp, fits the network."""
    model, report = out.with_suffix(".txt"), out.with_suffix(".json")
    cmd = [
        sys.executable,
        "-m",
        "ml.human_model.logit",
        "--features",
        feats,
        "--store",
        store,
        "--out",
        model,
        "--report",
        report,
        "--movers",
        cfg.movers,
        "--max-train",
        n_train,
        "--search-train",
        min(n_train, 20000),
        "--max-val",
        cfg.max_val,
        "--top-k",
        cfg.top_k,
        "--l2",
        cfg.l2,
        "--l1",
        cfg.l1,
    ]
    if cfg.drop:
        cmd += ["--drop", cfg.drop]
    if final:
        cmd += ["--test"] + (["--mlp", "--mlp-hidden", cfg.mlp_hidden or 32] if cfg.mlp else [])
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


def train(cfg: Config, work: Path = DEFAULT_WORK, limits: Limits | None = None, log=print) -> Path:
    """Generates, prepares and fits; returns the new version's directory."""
    limits = limits or Limits()
    start = time.time()
    if cfg.source == "human":
        cfg = replace(cfg, movers="human")
    data = data_dir(work, cfg)
    cfg = cfg.with_level_model()
    est = estimate(cfg)
    log(
        f"level {cfg.level}: {cfg.size()} {cfg.source} games in {data} "
        f"(about {est['hours']} h on 2 cores, {est['gb']} GB)"
    )
    if cfg.source == "human":
        feats, raw, games = prepare_human(cfg, data, limits)
        store = Path(cfg.human_store)
    else:
        games = generate(cfg, data, limits)
        if games < cfg.size():
            raise RuntimeError(f"only {games} of {cfg.size()} games were played")
        feats = prepare(cfg, data, limits)
        store, raw = data / f"store_{cfg.size()}", data / f"raw_{cfg.size()}"
    n = min(train_turns(feats, store, cfg), cfg.max_train)
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
        final = fraction == cfg.fractions[-1]
        name = tmp / f"fit_{round(100 * fraction)}"
        report = fit(cfg, feats, store, name, size, limits, final=final)
        curve.append(
            {
                "fraction": fraction,
                "train_turns": report["train_turns"],
                "val_turns": report["val_turns"],
                "val_nll": report["val_nll_logit"],
            }
        )
        log(
            f"  {round(100 * fraction)} %: {report['train_turns']} turns, "
            f"val NLL {report['val_nll_logit']:.4f}"
        )
    shutil.copy(tmp / f"fit_{round(100 * cfg.fractions[-1])}.txt", tmp / "model.txt")
    (tmp / "curve.json").write_text(json.dumps(curve, indent=1) + "\n")
    test = {k: v for k, v in report.items() if k.startswith(("test_", "val_nll_mlp", "kind"))}
    test["test_nll_uniform"] = uniform_test_nll(feats, store, cfg)
    if test.get("test_nll_logit") is not None:
        log(
            f"  test NLL: logit {test['test_nll_logit']:.4f}"
            + (f", mlp {test['test_nll_mlp']:.4f}" if "test_nll_mlp" in test else "")
            + f", uniform {test['test_nll_uniform']:.4f} ({report['kind']} kept)"
        )
    config = (
        asdict(cfg)
        | test
        | {
            "games_played": games,
            "data": str(data),
            "features": str(feats),
            "raw": str(raw),
            "store": str(store),
            "train_turns": n,
            "commit": _commit(),
            "seconds": round(time.time() - start, 1),
            "still_dropping": still_dropping(curve),
        }
    )
    (tmp / "config.json").write_text(json.dumps(config, indent=1) + "\n")
    tmp.rename(version)
    log(f"saved {version}")
    tracking.log_training(version)
    return version


def _commit() -> str:
    r = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True, cwd=ROOT
    )
    return r.stdout.strip()


def config_args(ap: argparse.ArgumentParser) -> None:
    """The options shared by ml.train and the menu."""
    ap.add_argument("--level", type=int, choices=sorted(LEVELS), default=0)
    ap.add_argument("--games", type=int, default=0, help="any number of games instead")
    ap.add_argument("--lexicon", default=str(DEFAULT_LEXICON))
    ap.add_argument("--leaves", default=str(DEFAULT_LEAVES))
    ap.add_argument("--bots", default="equity,equity", help="the two self-play bots")
    ap.add_argument("--human-params", default="", help="model of the 'human' bot")
    ap.add_argument(
        "--source",
        default="selfplay",
        choices=("selfplay", "human"),
        help="self-play games, or the downloaded human games (human movers)",
    )
    ap.add_argument("--movers", default="all", choices=("all", "human", "bot"))
    ap.add_argument("--top-k", default="64,128", help="top_k values the fit tries on val")
    ap.add_argument("--l2", default="1e-2", help="L2 strengths the fit tries")
    ap.add_argument(
        "--max-train", type=int, default=0, help="turns of the largest fit (0: the level's)"
    )
    ap.add_argument("--estimate", action="store_true", help="print the run's estimate and exit")
    ap.add_argument(
        "--fractions",
        default="0.25,0.5,1",
        help="shares of --max-train fitted (the learning curve)",
    )
    ap.add_argument("--mlp", action="store_true", help="also fit the network, kept if better")
    ap.add_argument("--drop", default="", help="feature groups to leave out, e.g. A")
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
    cfg = Config(
        level=args.level,
        games=args.games,
        lexicon=args.lexicon,
        leaves=args.leaves,
        bots=bots,
        human_params=args.human_params,
        source=args.source,
        movers=args.movers,
        top_k=args.top_k,
        l2=args.l2,
        max_train=args.max_train,
        fractions=tuple(float(f) for f in args.fractions.split(",")),
        mlp=args.mlp,
        drop=args.drop,
        min_rating=args.min_rating,
        max_rating=args.max_rating,
        seed=args.seed,
    )
    return cfg, Limits(args.threads, args.mem)


def check_busy(force: bool) -> None:
    others = busy()
    if others and not force:
        raise SystemExit(
            "another heavy run is active; wait for it or pass --force:\n  " + "\n  ".join(others)
        )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    config_args(ap)
    args = ap.parse_args()
    cfg, limits = config_from(args)
    if args.estimate:
        print(json.dumps(estimate(cfg)))
        return 0
    check_busy(args.force)
    train(cfg, args.work, limits)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
