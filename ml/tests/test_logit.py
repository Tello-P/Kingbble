import pytest

from ml.data.features import FEATURES, Turn
from ml.human_model.logit import band_of, fit, fit_bands, write_params

np = pytest.importorskip("numpy")


def synthetic(w, turns=1500, options=12, seed=0):
    rng = np.random.default_rng(seed)
    groups = []
    for _ in range(turns):
        x = rng.normal(size=(options, len(w)))
        u = x @ w
        p = np.exp(u - u.max())
        groups.append((x, int(rng.choice(options, p=p / p.sum()))))
    return groups


def test_fit_recovers_the_weights():
    w = np.zeros(len(FEATURES))
    w[0], w[1] = -1.0, 0.5
    est = fit(synthetic(w))
    assert np.abs(est - w).max() < 0.15


def test_fit_converges_when_the_best_option_is_almost_always_played():
    # A few turns like real ones (gap in points, log rank); a full Newton step diverged here.
    rng = np.random.default_rng(1)
    groups = []
    for _ in range(50):
        x = np.zeros((64, len(FEATURES)))
        x[:, 0] = np.sort(rng.exponential(10.0, 64))
        x[0, 0] = 0
        x[:, 1] = rng.integers(0, 60, 64)
        x[:, 3] = np.log1p(np.arange(64))
        groups.append((x, 0 if rng.random() < 0.9 else int(rng.integers(1, 5))))
    est = fit(groups)
    assert np.isfinite(est).all()
    assert np.abs(est).max() < 100


def test_band_of():
    assert band_of(None, (0, 1500)) == 0
    assert band_of(1499, (0, 1500)) == 0
    assert band_of(2000, (0, 1500)) == 1


def test_fit_bands_and_residual(tmp_path):
    w = np.zeros(len(FEATURES))
    w[0] = -2.0
    groups = synthetic(w, turns=400, options=4)
    rows = np.concatenate([x for x, _ in groups]).astype("<f4")
    turns = [Turn("g", i, 0, "", 80, 50, y, 4 * i, 4) for i, (_, y) in enumerate(groups)]
    # One played option below the top rows, one we do not generate.
    turns += [Turn("g", 400, 0, "", 80, 50, 30, 0, 5), Turn("g", 401, 0, "", 80, 50, -1, 0, 4)]
    weights, residual = fit_bands(turns, rows, lambda t: 1600 if t.turn % 2 else 1200, (0, 1500))
    assert residual == pytest.approx(1 / 401)
    for band in weights:
        assert band[0] < -1.0
    out = tmp_path / "m.txt"
    write_params(out, weights, (0, 1500), 4, 0.05, residual)
    lines = out.read_text().splitlines()
    assert lines[1:4] == ["top_k 4", "epsilon 0.05", f"residual {residual:.6g}"]
    assert lines[4].startswith("band 0 ") and len(lines[5].split()) == 2 + len(FEATURES)
