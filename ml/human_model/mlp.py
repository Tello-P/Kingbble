"""Small network scoring a turn's candidates, trained with a softmax over them.

u(x) = w2 . relu(W1 z + b1) + v . z, z the standardized features; P(candidate)
is the softmax of u / T over the turn's top-k candidates. The linear skip v
starts from the logit's weights (or zero) and the hidden layer learns what the
logit misses; T is fitted
on val after training (temperature scaling). Trained by Adam on the inside
turns with early stopping on val. The skip is exported as two more hidden
units (v . z = relu(v . z) - relu(-v . z)), so the engine reads one hidden
layer (HumanModelParams::Mlp).
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from ml.human_model.dataset import Scaler, TurnSet, log_softmax_chosen


@dataclass
class Net:
    w1: np.ndarray  # (hidden, features)
    b1: np.ndarray
    w2: np.ndarray
    v: np.ndarray  # linear skip
    temperature: float = 1.0
    epochs: int = 0

    @property
    def hidden(self) -> int:
        return len(self.b1)

    def utility(self, z: np.ndarray) -> np.ndarray:
        return (np.maximum(z @ self.w1.T + self.b1, 0) @ self.w2 + z @ self.v) / self.temperature


def log_p(net: Net, ts: TurnSet, scaler: Scaler) -> np.ndarray:
    """Softmax log probability of the played candidate per turn (0 for turns outside)."""
    out = np.zeros(len(ts))
    for chunk in ts.chunks(np.flatnonzero(ts.inside)):
        x, starts = ts.gather(chunk)
        out[chunk] = log_softmax_chosen(net.utility(scaler(x)), starts, ts.chosen[chunk])
    return out


def _step(net: Net, z: np.ndarray, starts: np.ndarray, chosen: np.ndarray, decay: float):
    """(mean NLL, gradients) for one batch."""
    a = z @ net.w1.T + net.b1
    h = np.maximum(a, 0)
    u = h @ net.w2 + z @ net.v
    lens = np.diff(np.append(starts, len(u)))
    ids = np.repeat(np.arange(len(starts)), lens)
    m = np.maximum.reduceat(u, starts)
    e = np.exp(u - m[ids])
    zsum = np.add.reduceat(e, starts)
    p = e / zsum[ids]
    rows = starts + chosen
    nll = float((np.log(zsum) + m - u[rows]).mean())
    du = p
    du[rows] -= 1
    du /= len(starts)
    dh = np.outer(du, net.w2) * (a > 0)
    grads = {
        "w1": dh.T @ z + decay * net.w1,
        "b1": dh.sum(0),
        "w2": h.T @ du + decay * net.w2,
        "v": z.T @ du,
    }
    return nll, grads


def fit_temperature(net: Net, val: TurnSet, scaler: Scaler) -> float:
    """T minimizing the val softmax NLL of u / T (golden section on log T)."""
    base = net.temperature
    chunks = [(scaler(x), s, val.chosen[c])
              for c in val.chunks(np.flatnonzero(val.inside))
              for x, s in [val.gather(c)]]
    net.temperature = 1.0
    raw = [(net.utility(z), s, ch) for z, s, ch in chunks]

    def nll(log_t):
        t = np.exp(log_t)
        return -np.mean(np.concatenate([log_softmax_chosen(u / t, s, ch) for u, s, ch in raw]))

    lo, hi = -1.5, 1.5
    g = (np.sqrt(5) - 1) / 2
    a, b = hi - g * (hi - lo), lo + g * (hi - lo)
    fa, fb = nll(a), nll(b)
    for _ in range(30):
        if fa < fb:
            hi, b, fb = b, a, fa
            a = hi - g * (hi - lo)
            fa = nll(a)
        else:
            lo, a, fa = a, b, fb
            b = lo + g * (hi - lo)
            fb = nll(b)
    net.temperature = base
    return float(np.exp((lo + hi) / 2))


def fit(train: TurnSet, val: TurnSet, scaler: Scaler, hidden: int = 32, epochs: int = 10,
        batch: int = 256, lr: float = 5e-4, decay: float = 1e-3, patience: int = 3,
        seed: int = 0, skip=None, on_epoch=None) -> Net:
    """skip: initial linear weights (standardized features), e.g. the logit's."""
    rng = np.random.default_rng(seed)
    d = len(train.cols)
    net = Net(w1=rng.normal(0, np.sqrt(2 / d), (hidden, d)), b1=np.zeros(hidden),
              w2=rng.normal(0, 0.01 / np.sqrt(hidden), hidden),
              v=np.zeros(d) if skip is None else np.array(skip, dtype=float))
    params = ("w1", "b1", "w2", "v")
    m = {k: np.zeros_like(getattr(net, k)) for k in params}
    s = {k: np.zeros_like(getattr(net, k)) for k in params}
    inside = np.flatnonzero(train.inside)
    # The starting point (the logit, with a near-silent hidden layer) is the bar.
    best_nll, bad, t = -log_p(net, val, scaler)[val.inside].mean(), 0, 0
    best = Net(*(getattr(net, k).copy() for k in params), epochs=0)
    for epoch in range(epochs):
        order = rng.permutation(inside)
        rate = lr * 0.7**epoch
        for i in range(0, len(order), batch):
            idx = np.sort(order[i : i + batch])
            x, starts = train.gather(idx)
            _, grads = _step(net, scaler(x), starts, train.chosen[idx], decay)
            t += 1
            for k in params:
                m[k] = 0.9 * m[k] + 0.1 * grads[k]
                s[k] = 0.999 * s[k] + 0.001 * grads[k] ** 2
                step = rate * (m[k] / (1 - 0.9**t)) / (np.sqrt(s[k] / (1 - 0.999**t)) + 1e-8)
                setattr(net, k, getattr(net, k) - step)
        val_nll = -log_p(net, val, scaler)[val.inside].mean()
        if on_epoch:
            on_epoch(epoch, val_nll)
        if val_nll < best_nll - 1e-4:
            best = Net(*(getattr(net, k).copy() for k in params), epochs=epoch + 1)
            best_nll, bad = val_nll, 0
        else:
            bad += 1
            if bad >= patience:
                break
    best.temperature = fit_temperature(best, val, scaler)
    return best


def param_lines(net: Net, scaler: Scaler) -> list[str]:
    """The mlp_* lines of a model file (HumanModelParams::FromFile)."""
    w1 = np.vstack([net.w1, net.v, -net.v])
    b1 = np.concatenate([net.b1, [0.0, 0.0]])
    w2 = np.concatenate([net.w2, [1.0, -1.0]])

    def fmt(a):
        return " ".join(f"{x:.6g}" for x in np.ravel(a))

    return [
        f"mlp_hidden {len(b1)}",
        f"mlp_temperature {net.temperature:.6g}",
        f"mlp_mean {fmt(scaler.mean)}",
        f"mlp_scale {fmt(scaler.scale)}",
        f"mlp_w1 {fmt(w1)}",
        f"mlp_b1 {fmt(b1)}",
        f"mlp_w2 {fmt(w2)}",
    ]
