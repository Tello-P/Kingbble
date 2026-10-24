import pytest

np = pytest.importorskip("numpy")

from ml.human_model import mlp  # noqa: E402
from ml.human_model.dataset import Scaler  # noqa: E402
from ml.tests.test_logit import sets  # noqa: E402


def test_network_learns_a_nonlinear_preference_and_exports():
    # Players avoid options whose second feature is far from 0 in either direction.
    rng = np.random.default_rng(3)
    w = np.array([-1.0, 0.0])
    train, val = sets(w, turns=1200, options=8, seed=2)
    rows = np.array(train.options)
    rows[:, 1] = rng.normal(size=len(rows))
    # Redraw the choices with the nonlinear utility.
    for ts in (train, val):
        for t in range(len(ts)):
            f, k = ts.first[t], ts.k[t]
            u = -1.0 * rows[f : f + k, 0] - 1.5 * np.abs(rows[f : f + k, 1])
            p = np.exp(u - u.max())
            ts.chosen[t] = rng.choice(k, p=p / p.sum())
        ts.options = rows
    scaler = Scaler.fit(train)
    net = mlp.fit(train, val, scaler, hidden=8, epochs=6, lr=1e-2)
    nll = -mlp.log_p(net, val, scaler)[val.inside].mean()
    assert nll < np.log(8) - 0.3
    assert net.temperature > 0
    lines = mlp.param_lines(net, scaler)
    assert lines[0] == "mlp_hidden 10"
    w1 = np.array(lines[4].split()[1:], dtype=float).reshape(10, 2)
    b1 = np.array(lines[5].split()[1:], dtype=float)
    w2 = np.array(lines[6].split()[1:], dtype=float)
    z = scaler(rows[:5].astype(float))
    exported = (np.maximum(z @ w1.T + b1, 0) @ w2) / net.temperature
    assert np.allclose(exported, net.utility(z), atol=1e-4)
