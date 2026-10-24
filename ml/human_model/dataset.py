"""Turns of a feature file (ml.data.features) as arrays for the human move models.

A turn's candidates are its top k rows by static equity. A turn is "inside"
when the played option is one of them; the others (played option ranked lower,
or one we do not generate) give the residual mass of the mixture.
"""

from __future__ import annotations

from collections.abc import Iterator, Sequence
from dataclasses import dataclass, replace

import numpy as np

from ml.data.split import split_of

# Feature groups by name, for ablations. "base" is never dropped.
GROUPS = {
    "base": ("gap", "score", "leave", "log_rank", "tiles", "bingo", "exchange", "pass"),
    "A": ("word_zipf", "word_plays", "word_unfamiliar", "word_length", "word_alt",
          "word_log_prob", "rack_anagrams"),
    "B": ("had_bingo", "bingo_count", "bingo_easiest", "bingo_gap", "missed_bingo"),
    "C": ("leave_size", "leave_vowels", "leave_consonants", "leave_duplicates", "leave_s",
          "leave_blank", "leave_q_no_u", "leave_synergy", "leave_bingo_prob", "leave_n_blank",
          *(f"leave_n_{chr(c)}" for c in range(ord("a"), ord("z") + 1))),
    "D": ("on_dl", "on_tl", "on_dw", "on_tw", "cross_words", "hooks", "through", "center_dist",
          "vertical"),
    "E": ("opens_tw", "bingo_lanes"),
    "F": ("score_rank", "equity_z", "second_gap", "non_bingo_gap"),
    "G": ("score_diff", "bag_tiles", "turn_number", "endgame", "prev_exchange", "rating",
          "rating_known"),
    "H": ("gap_x_phase", "score_x_phase", "leave_x_phase", "bingo_x_phase", "exchange_x_phase",
          "gap_x_rating", "log_rank_x_rating", "missed_bingo_x_rating", "zipf_x_rating",
          "gap_x_lead", "score_x_lead", "exchange_x_lead"),
}


def columns(names: Sequence[str], drop: Sequence[str] = ()) -> list[int]:
    """Indices of the features kept when the groups in `drop` are left out."""
    dropped = {n for g in drop for n in GROUPS[g]}
    return [i for i, n in enumerate(names) if n not in dropped]


def band_of(rating: int | None, bands: Sequence[int]) -> int:
    """Index of the band a rating falls in; unknown ratings go to the first."""
    band = 0
    for i, low in enumerate(bands):
        if rating is not None and rating >= low:
            band = i
    return band


@dataclass
class TurnSet:
    options: np.ndarray  # (rows, features), usually a memmap
    cols: np.ndarray  # feature columns in use
    first: np.ndarray  # first row of each turn
    k: np.ndarray  # candidate rows of each turn
    chosen: np.ndarray  # played row in [0, k), -1 outside the candidates or unknown
    n: np.ndarray  # legal options
    band: np.ndarray

    def __len__(self) -> int:
        return len(self.first)

    @property
    def inside(self) -> np.ndarray:
        return self.chosen >= 0

    def subset(self, idx) -> TurnSet:
        idx = np.asarray(idx)
        return replace(self, first=self.first[idx], k=self.k[idx], chosen=self.chosen[idx],
                       n=self.n[idx], band=self.band[idx])

    def with_top(self, top_k: int) -> TurnSet:
        """The same turns with at most top_k candidates each."""
        k = np.minimum(self.k, top_k)
        return replace(self, k=k, chosen=np.where(self.chosen < k, self.chosen, -1))

    def with_cols(self, cols) -> TurnSet:
        return replace(self, cols=np.asarray(cols))

    def coverage(self) -> float:
        """Share of turns whose played option is among the candidates."""
        return float(self.inside.mean()) if len(self) else 0.0

    def gather(self, idx) -> tuple[np.ndarray, np.ndarray]:
        """(rows of the turns idx stacked as float64, start of each turn in them)."""
        idx = np.asarray(idx)
        k = self.k[idx]
        starts = np.concatenate([[0], np.cumsum(k)[:-1]]).astype(np.int64)
        x = np.empty((int(k.sum()), len(self.cols)))
        for s, f, n in zip(starts, self.first[idx], k, strict=True):
            x[s : s + n] = self.options[f : f + n][:, self.cols]
        return x, starts

    def chunks(self, idx=None, size: int = 2000) -> Iterator[np.ndarray]:
        idx = np.arange(len(self)) if idx is None else np.asarray(idx)
        for i in range(0, len(idx), size):
            yield idx[i : i + size]


def select(turns, options, info: dict, split: str, top_k: int, bands: Sequence[int],
           movers: str = "human", max_turns: int = 0, seed: int = 0) -> TurnSet:
    """Turns of `split` (info: (game_id, turn) -> (rating, split, is_bot)) played by
    `movers` ("human", "bot" or "all"), at most max_turns of them (a seeded random
    subset, kept in file order) when > 0. A few bot accounts play every bot turn,
    so bot turns are split by game instead of by player."""
    keep, ratings = [], []
    for t in turns:
        rating, s, bot = info.get((t.game_id, t.turn), (None, "", False))
        if bot and movers == "bot":
            s = split_of(t.game_id)
        if s != split or (movers == "human" and bot) or (movers == "bot" and not bot):
            continue
        keep.append(t)
        ratings.append(rating)
    if max_turns and len(keep) > max_turns:
        pick = np.sort(np.random.default_rng(seed).choice(len(keep), max_turns, replace=False))
        keep = [keep[i] for i in pick]
        ratings = [ratings[i] for i in pick]
    k = np.array([min(t.top, top_k) for t in keep], dtype=np.int64)
    chosen = np.array([t.chosen for t in keep], dtype=np.int64)
    chosen = np.where((chosen >= 0) & (chosen < k), chosen, -1)
    return TurnSet(
        options=options,
        cols=np.arange(options.shape[1]),
        first=np.array([t.first_row for t in keep], dtype=np.int64),
        k=k,
        chosen=chosen,
        n=np.array([t.options for t in keep], dtype=np.int64),
        band=np.array([band_of(r, bands) for r in ratings], dtype=np.int64),
    )


@dataclass
class Scaler:
    mean: np.ndarray
    scale: np.ndarray  # 1 where a feature is constant

    @staticmethod
    def fit(ts: TurnSet, max_turns: int = 20000, seed: int = 0) -> Scaler:
        idx = np.arange(len(ts))
        if len(idx) > max_turns:
            idx = np.sort(np.random.default_rng(seed).choice(idx, max_turns, replace=False))
        total = np.zeros(len(ts.cols))
        total_sq = np.zeros(len(ts.cols))
        count = 0
        for chunk in ts.chunks(idx):
            x, _ = ts.gather(chunk)
            total += x.sum(0)
            total_sq += (x * x).sum(0)
            count += len(x)
        mean = total / max(count, 1)
        var = np.maximum(total_sq / max(count, 1) - mean**2, 0)
        scale = np.sqrt(var)
        return Scaler(mean, np.where(scale > 1e-9, scale, 1.0))

    def __call__(self, x: np.ndarray) -> np.ndarray:
        return (x - self.mean) / self.scale


def log_softmax_chosen(u: np.ndarray, starts: np.ndarray, chosen: np.ndarray) -> np.ndarray:
    """log P(chosen row) per turn from stacked utilities (chosen >= 0 rows only)."""
    m = np.maximum.reduceat(u, starts)
    ids = np.repeat(np.arange(len(starts)), np.diff(np.append(starts, len(u))))
    lse = np.log(np.add.reduceat(np.exp(u - m[ids]), starts)) + m
    return u[starts + np.maximum(chosen, 0)] - lse


def mixture_nll(log_p: np.ndarray, ts: TurnSet, epsilon: float, residual: float) -> float:
    """Mean negative log-likelihood of the played options under the engine's mixture
    (HumanMoveModel::OptionProbabilities); log_p is the softmax log probability of the
    played candidate, ignored for turns outside."""
    uniform = epsilon / (ts.n + 1)
    outside = (1 - epsilon) * residual / np.maximum(ts.n - ts.k + 1, 1) + uniform
    inside = (1 - epsilon) * (1 - residual) * np.exp(np.where(ts.inside, log_p, 0)) + uniform
    p = np.where(ts.inside, inside, outside)
    return float(-np.log(np.maximum(p, 1e-300)).mean())


def tune_mixture(log_p: np.ndarray, ts: TurnSet, epsilons=None, residuals=None):
    """(epsilon, residual, nll) minimizing mixture_nll on a grid."""
    if epsilons is None:
        epsilons = (0.0, 0.001, 0.003, 0.01, 0.02, 0.03, 0.05, 0.07, 0.1, 0.15)
    if residuals is None:
        share = max(1.0 - ts.coverage(), 1e-4)
        residuals = sorted({min(0.5, share * f) for f in (0.5, 0.75, 1.0, 1.25, 1.5, 2.0)})
    best = None
    for e in epsilons:
        for r in residuals:
            nll = mixture_nll(log_p, ts, e, r)
            if best is None or nll < best[2]:
                best = (e, r, nll)
    return best


def tune_band_mixtures(log_p: np.ndarray, ts: TurnSet, n_bands: int, fallback):
    """([(epsilon, residual)] per band, mean nll): tune_mixture within each band; a band
    without turns gets `fallback`."""
    mixes, total = [], 0.0
    for b in range(n_bands):
        idx = np.flatnonzero(ts.band == b)
        if not len(idx):
            mixes.append(tuple(fallback))
            continue
        e, r, nll = tune_mixture(log_p[idx], ts.subset(idx))
        mixes.append((e, r))
        total += nll * len(idx)
    return mixes, total / max(len(ts), 1)


def banded_mixture_nll(log_p: np.ndarray, ts: TurnSet, mixes) -> float:
    """mixture_nll with each band's (epsilon, residual)."""
    total = 0.0
    for b, (e, r) in enumerate(mixes):
        idx = np.flatnonzero(ts.band == b)
        if len(idx):
            total += mixture_nll(log_p[idx], ts.subset(idx), e, r) * len(idx)
    return total / max(len(ts), 1)
