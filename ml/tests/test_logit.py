import pytest

np = pytest.importorskip("numpy")

from ml.data.features import Turn  # noqa: E402
from ml.human_model.dataset import Scaler, mixture_nll, select, tune_mixture  # noqa: E402
from ml.human_model.logit import (  # noqa: E402
    band_of,
    fit,
    fit_bands,
    in_rating_range,
    log_p,
    raw_weights,
    write_params,
)


def synthetic(w, turns=1500, options=12, seed=0, rating=lambda i: 1600):
    """(turns, rows, info) of a conditional logit with weights w."""
    rng = np.random.default_rng(seed)
    rows, out, info = [], [], {}
    for i in range(turns):
        x = rng.normal(size=(options, len(w))) * np.arange(1, len(w) + 1)
        u = x @ w
        p = np.exp(u - u.max())
        chosen = int(rng.choice(options, p=p / p.sum()))
        out.append(Turn("g", i, 0, "", 80, 50, chosen, i * options, options))
        info[("g", i)] = (rating(i), "train" if i % 4 else "val", False)
        rows.append(x)
    return out, np.concatenate(rows).astype("<f4"), info


def sets(w, **kw):
    turns, rows, info = synthetic(w, **kw)
    train = select(turns, rows, info, "train", 64, (0, 1500))
    val = select(turns, rows, info, "val", 64, (0, 1500))
    return train, val


def test_fit_recovers_the_weights_on_the_raw_scale():
    w = np.array([-1.0, 0.25, 0.0])
    train, _ = sets(w)
    scaler = Scaler.fit(train)
    est = raw_weights(fit(train, scaler, l2=1e-6), scaler)
    assert np.abs(est - w).max() < 0.1


def test_l1_zeroes_a_useless_feature():
    w = np.array([-1.0, 0.25, 0.0])
    train, _ = sets(w)
    scaler = Scaler.fit(train)
    est = fit(train, scaler, l2=1e-4, l1=0.08)
    assert est[2] == 0 and est[0] < 0


def test_bands_shrink_towards_the_common_model():
    w = np.array([-1.0, 0.0, 0.0])
    train, val = sets(w, rating=lambda i: 1200 if i % 2 else 1800)
    scaler = Scaler.fit(train)
    common = fit(train, scaler)
    weights, shrink = fit_bands(train, val, scaler, common, 2, shrinks=(1e3, 1e-3))
    assert weights.shape == (2, 3) and shrink in (1e3, 1e-3)
    tight, _ = fit_bands(train, val, scaler, common, 2, shrinks=(1e3,))
    assert np.abs(tight - common).max() < 1e-2


def test_band_of():
    assert band_of(None, (0, 1500)) == 0
    assert band_of(1499, (0, 1500)) == 0
    assert band_of(2000, (0, 1500)) == 1


def test_mixture_matches_the_engine_formula():
    turns = [
        Turn("g", 0, 0, "", 80, 10, 1, 0, 4),
        Turn("g", 1, 0, "", 80, 10, 30, 4, 5),
        Turn("g", 2, 0, "", 80, 10, -1, 9, 4),
    ]
    rows = np.zeros((13, 2), dtype="<f4")
    info = {("g", i): (None, "train", False) for i in range(3)}
    ts = select(turns, rows, info, "train", 4, (0,))
    assert ts.chosen.tolist() == [1, -1, -1]
    log_p = np.array([np.log(0.25), 0.0, 0.0])
    e, r = 0.1, 0.2
    inside = 0.9 * 0.8 * 0.25 + 0.1 / 11
    outside = 0.9 * 0.2 / 7 + 0.1 / 11
    expected = -(np.log(inside) + 2 * np.log(outside)) / 3
    assert mixture_nll(log_p, ts, e, r) == pytest.approx(expected)
    best_e, best_r, nll = tune_mixture(log_p, ts)
    assert nll <= mixture_nll(log_p, ts, e, r) + 1e-12


def test_log_p_and_write_params(tmp_path):
    w = np.array([-1.0, 0.25, 0.0])
    train, val = sets(w, turns=200, options=4)
    scaler = Scaler.fit(train)
    common = fit(train, scaler)
    lp = log_p(val, scaler, common)
    assert (lp[val.inside] <= 0).all()
    out = tmp_path / "m.txt"
    write_params(
        out,
        ["gap", "score", "opens_tw"],
        [raw_weights(common, scaler)] * 2,
        (0, 1500),
        4,
        0.05,
        0.01,
    )
    lines = out.read_text().splitlines()
    assert lines[1:7] == [
        "version 2",
        "kind logit",
        "features gap score opens_tw",
        "top_k 4",
        "epsilon 0.05",
        "residual 0.01",
    ]
    assert lines[7].startswith("band 0 ") and len(lines[8].split()) == 2 + 3


def test_select_picks_humans_or_bots_and_splits_bots_by_game():
    from ml.data.split import split_of

    turns = [Turn(f"g{i}", 0, 0, "", 80, 10, 0, 4 * i, 4) for i in range(40)]
    rows = np.zeros((160, 2), dtype="<f4")
    info = {(f"g{i}", 0): (1500, "train", i % 2 == 1) for i in range(40)}
    humans = select(turns, rows, info, "train", 4, (0,))
    assert len(humans) == 20
    bots = [select(turns, rows, info, s, 4, (0,), movers="bot") for s in ("train", "val", "test")]
    assert sum(len(b) for b in bots) == 20
    expected = sum(split_of(f"g{i}") == "train" for i in range(1, 40, 2))
    assert len(bots[0]) == expected
    assert len(select(turns, rows, info, "train", 4, (0,), movers="all")) == 40


def test_band_mixtures_are_tuned_within_each_band():
    from ml.human_model.dataset import banded_mixture_nll, tune_band_mixtures

    turns = [Turn("g", i, 0, "", 80, 10, 0 if i % 2 else 7, 4 * i, 4) for i in range(40)]
    rows = np.zeros((160, 2), dtype="<f4")
    info = {("g", i): (1200 if i < 20 else 1800, "train", False) for i in range(40)}
    ts = select(turns, rows, info, "train", 4, (0, 1500))
    lp = np.where(ts.inside, np.log(0.25), 0.0)
    mixes, nll = tune_band_mixtures(lp, ts, 3, (0.05, 0.1))
    assert len(mixes) == 3 and mixes[2] == (0.05, 0.1)
    assert banded_mixture_nll(lp, ts, mixes) == pytest.approx(nll)
    assert nll <= mixture_nll(lp, ts, 0.05, 0.1) + 1e-12


def test_rating_range_keeps_unrated_turns():
    info = {("g", 1): (1200, "train", False), ("g", 2): (1800, "train", False),
            ("g", 3): (None, "train", True)}
    assert set(in_rating_range(info, 1500, None)) == {("g", 2), ("g", 3)}
    assert set(in_rating_range(info, None, 1500)) == {("g", 1), ("g", 3)}
    assert in_rating_range(info, None, None) == info
