import pytest

np = pytest.importorskip("numpy")

from ml.human_model.dataset import TurnSet, log_softmax_chosen  # noqa: E402
from ml.human_model.lgbm import fit_temperature, ranking_data, scores, softmax_log_p  # noqa: E402


def turn_set(rows, k, chosen):
    first = np.concatenate([[0], np.cumsum(k)[:-1]])
    return TurnSet(
        options=np.asarray(rows, dtype=float),
        cols=np.arange(np.asarray(rows).shape[1]),
        first=first.astype(np.int64),
        k=np.asarray(k, dtype=np.int64),
        chosen=np.asarray(chosen, dtype=np.int64),
        n=np.asarray(k, dtype=np.int64) + 5,
        band=np.zeros(len(k), dtype=np.int64),
    )


class Linear:
    def __init__(self, w):
        self.w = np.asarray(w)

    def predict(self, x):
        return x @ self.w


def test_ranking_data_marks_the_played_candidate_and_skips_turns_outside():
    ts = turn_set([[1], [2], [3], [4], [5]], [3, 2], [1, -1])
    x, y, groups = ranking_data(ts)
    assert x.tolist() == [[1], [2], [3]]
    assert y.tolist() == [0, 1, 0]
    assert groups.tolist() == [3]


def test_temperature_recovers_the_scale_of_the_scores():
    rng = np.random.default_rng(0)
    k = np.full(400, 6)
    x = rng.normal(size=(int(k.sum()), 1))
    first = np.concatenate([[0], np.cumsum(k)[:-1]])
    chosen = []
    for f, n in zip(first, k, strict=True):
        u = 2.0 * x[f : f + n, 0]  # true utilities: twice the score
        p = np.exp(u - u.max())
        chosen.append(int(rng.choice(n, p=p / p.sum())))
    ts = turn_set(x, k, chosen)
    u, starts = scores(Linear([1.0]), ts)
    t = fit_temperature(u, starts, ts)
    assert 0.35 < t < 0.7  # about 1/2
    lp = softmax_log_p(u, starts, ts, t)
    assert lp.mean() > softmax_log_p(u, starts, ts, 1.0).mean()
    assert np.allclose(softmax_log_p(u, starts, ts, 1.0), log_softmax_chosen(u, starts, ts.chosen))
