from pathlib import Path

import pytest

from ml.eval import tournament_config as tc

ROOT = Path(__file__).resolve().parents[2]
M7 = ROOT / "configs" / "m7"


def test_merge_nested():
    base = {"sim": {"candidates": 10, "iterations": 200}, "games": 400}
    out = tc.merge(base, {"sim": {"iterations": 32}, "seed": 3})
    assert out == {"sim": {"candidates": 10, "iterations": 32}, "games": 400, "seed": 3}
    assert base["sim"]["iterations"] == 200


def test_m7_has_the_four_ablations():
    names = [p.stem for p in tc.experiments(M7)]
    assert names == ["endgame", "human_vs_rangefinder", "inference", "sim_vs_equity"]
    seeds = {tc.load(p)["seed"] for p in tc.experiments(M7)}
    assert len(seeds) == 4


def test_every_m7_run_is_reproducible():
    for path in tc.experiments(M7):
        for quick in (False, True):
            args = tc.selfplay_args(tc.load(path, quick), threads=2)
            assert "--paired" in args and "--per-game-seeds" in args
            assert "--sim-time" not in args
            assert args[args.index("--endgame-time") + 1] == "0"
            assert args[args.index("--endgame-threads") + 1] == "1"
            assert args[args.index("--threads") + 1] == "2"


def test_quick_overrides(tmp_path):
    (tmp_path / "common.yaml").write_text(
        "lexicon: l.txt\nleaves: l.klv2\ngames: 400\nsim: {candidates: 10, iterations: 200}\n"
        "quick: {games: 20, sim: {iterations: 32}}\n"
    )
    (tmp_path / "x.yaml").write_text("name: x\na: sim+inf:human\nb: sim\nseed: 5\n")
    full = tc.load(tmp_path / "x.yaml")
    quick = tc.load(tmp_path / "x.yaml", quick=True)
    assert "quick" not in full
    assert (full["games"], full["sim"]["iterations"]) == (400, 200)
    assert (quick["games"], quick["sim"]["iterations"], quick["sim"]["candidates"]) == (20, 32, 10)
    args = tc.selfplay_args(quick)
    assert args[args.index("--a") + 1] == "sim+inf:human"
    assert args[args.index("--sim-iterations") + 1] == "32"
    assert tc.config_hash(full) != tc.config_hash(quick)
    assert tc.experiments(tmp_path) == [tmp_path / "x.yaml"]


def test_rejects_bad_configs(tmp_path):
    (tmp_path / "a.yaml").write_text("name: a\na: sim\nb: equity\nseed: 1\ngames: 4\n")
    with pytest.raises(ValueError, match="lexicon"):
        tc.load(tmp_path / "a.yaml")
    (tmp_path / "b.yaml").write_text(
        "name: b\na: sim\nb: equity\nseed: 1\ngames: 3\nlexicon: x\nleaves: y\n"
    )
    with pytest.raises(ValueError, match="even"):
        tc.load(tmp_path / "b.yaml")
    timed = tc.merge(tc.load(M7 / "inference.yaml"), {"sim": {"time": 2}})
    with pytest.raises(ValueError, match="iterations"):
        tc.selfplay_args(timed)


def test_cli_args(capsys):
    assert tc.main(["args", str(M7 / "endgame.yaml"), "--quick", "--threads", "3"]) == 0
    lines = capsys.readouterr().out.splitlines()
    assert lines[lines.index("--b") + 1] == "sim+inf:human"
    assert lines[lines.index("--games") + 1] == "20"
