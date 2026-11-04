"""Paired tournament report: win % and spread of bot a per experiment.

  python -m ml.eval.tournament RESULTS... [--out DIR] [--alpha 0.05]

Each RESULTS file is a kingbble_selfplay --paired results file ("game N: S1 -
S2, a in seat K"; games 2k-1 and 2k share a deal with the seats swapped), with
its <results>.manifest.json when there is one. Per experiment, over the
complete pairs: a's win % and mean spread with paired 95 % CIs (Student t over
the pair means, so the luck of the deal cancels out) and two-sided p-values
against 50 % and 0. The p-values are Holm-corrected across the experiments
(win % and spread each as their own family), so "significant" holds for all of
them together at --alpha. Prints a markdown table; with --out also writes
tournament.md, tournament.csv and tournament.svg (CI plot) there.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import math
import re
from dataclasses import asdict, dataclass, field
from pathlib import Path

LINE = re.compile(r"^game (\d+): (-?\d+) - (-?\d+), a in seat ([12])")


def read_results(path: Path) -> dict[int, tuple[int, int]]:
    """game -> (score a, score b) from a selfplay results file."""
    out: dict[int, tuple[int, int]] = {}
    for line in Path(path).read_text().splitlines():
        m = LINE.match(line)
        if not m:
            continue
        g, s1, s2, seat = (int(x) for x in m.groups())
        out.setdefault(g, (s1, s2) if seat == 1 else (s2, s1))
    return out


def read_manifest(results: Path) -> dict:
    path = Path(f"{results}.manifest.json")
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text())
    except json.JSONDecodeError:
        return {}


def pairs(done: dict[int, tuple[int, int]]) -> list[tuple[float, float]]:
    """(a's win share, a's mean spread) per complete pair (games 2k-1 and 2k)."""
    out = []
    for g in sorted(done):
        if g % 2 == 0 or g + 1 not in done:
            continue
        games = (done[g], done[g + 1])
        win = sum(1.0 if a > b else 0.5 if a == b else 0.0 for a, b in games) / 2
        out.append((win, sum(a - b for a, b in games) / 2))
    return out


def _betacf(a: float, b: float, x: float) -> float:
    """Continued fraction of the incomplete beta function (modified Lentz)."""
    tiny, qab, qap, qam = 1e-300, a + b, a + 1, a - 1
    c, d = 1.0, 1 - qab * x / qap
    d = 1 / (d if abs(d) > tiny else tiny)
    h = d
    for m in range(1, 300):
        m2 = 2 * m
        for aa in (
            m * (b - m) * x / ((qam + m2) * (a + m2)),
            -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2)),
        ):
            d = 1 + aa * d
            d = 1 / (d if abs(d) > tiny else tiny)
            c = 1 + aa / c
            c = c if abs(c) > tiny else tiny
            h *= d * c
        if abs(d * c - 1) < 1e-12:
            break
    return h


def betainc(a: float, b: float, x: float) -> float:
    """Regularized incomplete beta I_x(a, b)."""
    if x <= 0:
        return 0.0
    if x >= 1:
        return 1.0
    lbeta = math.lgamma(a + b) - math.lgamma(a) - math.lgamma(b)
    front = math.exp(lbeta + a * math.log(x) + b * math.log(1 - x))
    if x < (a + 1) / (a + b + 2):
        return front * _betacf(a, b, x) / a
    return 1 - front * _betacf(b, a, 1 - x) / b


def t_sf2(t: float, df: int) -> float:
    """Two-sided p-value of Student's t with df degrees of freedom."""
    return betainc(df / 2, 0.5, df / (df + t * t))


def t_quantile(p: float, df: int) -> float:
    """t with P(T <= t) = p (p > 0.5), by bisection on t_sf2."""
    target = 2 * (1 - p)
    lo, hi = 0.0, 1e3
    for _ in range(200):
        mid = (lo + hi) / 2
        if t_sf2(mid, df) > target:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2


def mean_ci(xs: list[float], null: float) -> tuple[float, float, float, float]:
    """Mean, 95 % CI and two-sided p-value against `null` (Student t)."""
    n = len(xs)
    mean = sum(xs) / n
    var = sum((x - mean) ** 2 for x in xs) / (n - 1)
    se = math.sqrt(var / n)
    half = t_quantile(0.975, n - 1) * se
    if se == 0:
        p = 1.0 if mean == null else 0.0
    else:
        p = t_sf2((mean - null) / se, n - 1)
    return mean, mean - half, mean + half, p


def holm(ps: list[float | None]) -> list[float | None]:
    """Holm-Bonferroni adjusted p-values (None stays None, left out of the family)."""
    idx = sorted((i for i, p in enumerate(ps) if p is not None), key=lambda i: ps[i])
    m = len(idx)
    out: list[float | None] = [None] * len(ps)
    running = 0.0
    for rank, i in enumerate(idx):
        running = max(running, min(1.0, (m - rank) * ps[i]))
        out[i] = running
    return out


@dataclass
class Row:
    name: str
    a: str = ""
    b: str = ""
    pairs: int = 0
    games: int = 0
    win: float | None = None
    win_lo: float | None = None
    win_hi: float | None = None
    p_win: float | None = None
    p_win_holm: float | None = None
    spread: float | None = None
    spread_lo: float | None = None
    spread_hi: float | None = None
    p_spread: float | None = None
    p_spread_holm: float | None = None
    significant: str = ""
    reproducible: str = ""
    commit: str = ""
    extra: dict = field(default_factory=dict, repr=False)


def experiment(path: Path) -> Row:
    """One results file summarized (before the Holm correction)."""
    manifest = read_manifest(path)
    config = manifest.get("config", {})
    tags = manifest.get("tags", {})
    name = Path(tags["config"]).stem if tags.get("config") else Path(path).stem
    row = Row(
        name=name,
        a=config.get("a", ""),
        b=config.get("b", ""),
        reproducible=config.get("reproducible", ""),
        commit=tags.get("commit", "")[:10] + ("+dirty" if tags.get("dirty") == "1" else ""),
    )
    ps = pairs(read_results(path))
    row.pairs, row.games = len(ps), 2 * len(ps)
    if len(ps) >= 2:
        w = mean_ci([p[0] for p in ps], 0.5)
        row.win, row.win_lo, row.win_hi = (100 * x for x in w[:3])
        row.p_win = w[3]
        row.spread, row.spread_lo, row.spread_hi, row.p_spread = mean_ci([p[1] for p in ps], 0.0)
    return row


def report(paths: list[Path], alpha: float = 0.05) -> list[Row]:
    rows = [experiment(p) for p in paths]
    for attr in ("p_win", "p_spread"):
        for row, adj in zip(rows, holm([getattr(r, attr) for r in rows]), strict=True):
            setattr(row, f"{attr}_holm", adj)
    for row in rows:
        if row.p_win_holm is None:
            row.significant = "too few pairs"
        elif row.p_win_holm < alpha:
            row.significant = "a better" if row.win > 50 else "a worse"
        elif row.p_spread_holm is not None and row.p_spread_holm < alpha:
            row.significant = "spread only: " + ("a better" if row.spread > 0 else "a worse")
        else:
            row.significant = "no"
    return rows


def _f(x: float | None, fmt: str) -> str:
    return "-" if x is None else format(x, fmt)


def _p(x: float | None) -> str:
    return "-" if x is None else ("<0.001" if x < 0.001 else f"{x:.3f}")


def markdown(rows: list[Row], alpha: float) -> str:
    head = (
        "| experiment | a | b | games | a win % (95 % CI) | p (Holm) "
        "| a spread (95 % CI) | p (Holm) | significant | reproducible | commit |\n"
        "|---|---|---|---:|---|---:|---|---:|---|---|---|\n"
    )
    lines = []
    for r in rows:
        win = f"{_f(r.win, '.1f')} ({_f(r.win_lo, '.1f')} to {_f(r.win_hi, '.1f')})"
        spread = f"{_f(r.spread, '+.1f')} ({_f(r.spread_lo, '+.1f')} to {_f(r.spread_hi, '+.1f')})"
        lines.append(
            f"| {r.name} | `{r.a}` | `{r.b}` | {r.games} | {win} | {_p(r.p_win_holm)} "
            f"| {spread} | {_p(r.p_spread_holm)} | {r.significant} "
            f"| {'yes' if r.reproducible == '1' else 'no' if r.reproducible else '?'} "
            f"| {r.commit or '?'} |"
        )
    note = (
        f"\nPaired over deals; p-values Holm-corrected across {len(rows)} experiments "
        f"(win % and spread separately), significant at alpha = {alpha}.\n"
    )
    return head + "\n".join(lines) + "\n" + note


CSV_FIELDS = [f for f in Row.__dataclass_fields__ if f != "extra"]


def to_csv(rows: list[Row]) -> str:
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=CSV_FIELDS)
    w.writeheader()
    for r in rows:
        d = asdict(r)
        d.pop("extra")
        w.writerow(d)
    return buf.getvalue()


def _esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def svg(rows: list[Row]) -> str:
    """Two panels (a's win % and spread), one row per experiment: the estimate
    with its 95 % CI against the no-difference line. Hover a row for numbers."""
    rows = [r for r in rows if r.win is not None]
    label_w, panel_w, gap, row_h, top, bottom = 190, 300, 40, 34, 46, 34
    width = label_w + 2 * panel_w + gap + 20
    height = top + row_h * max(len(rows), 1) + bottom
    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
        f'width="{width}" height="{height}" font-family="system-ui, sans-serif" font-size="12">',
        "<style>"
        ".bg{fill:#fcfcfb}.ink{fill:#0b0b0b}.ink2{fill:#52514e}.muted{fill:#898781}"
        ".grid{stroke:#e1e0d9}.ref{stroke:#898781}.ci{stroke:#2a78d6}.dot{fill:#2a78d6;"
        "stroke:#fcfcfb}.hit{fill:transparent}.hit:hover{fill:#2a78d614}"
        "@media (prefers-color-scheme: dark){.bg{fill:#1a1a19}.ink{fill:#f0efec}"
        ".ink2{fill:#c3c2b7}.grid{stroke:#2c2c2a}.ci{stroke:#5b9be6}"
        ".dot{fill:#5b9be6;stroke:#1a1a19}}"
        "</style>",
        f'<rect class="bg" width="{width}" height="{height}"/>',
    ]
    if not rows:
        out.append(f'<text class="muted" x="20" y="{top}">no experiment with 2+ pairs yet</text>')
        return "\n".join(out + ["</svg>"]) + "\n"

    def panel(x0: float, title: str, vals, null: float, fmt: str) -> None:
        lo = min([null] + [v[1] for v in vals])
        hi = max([null] + [v[2] for v in vals])
        pad = 0.08 * (hi - lo or 1)
        lo, hi = lo - pad, hi + pad

        def sx(v: float) -> float:
            return x0 + (v - lo) / (hi - lo) * panel_w

        out.append(f'<text class="ink" x="{x0}" y="18" font-weight="600">{_esc(title)}</text>')
        y_end = top + row_h * len(vals)
        step = _nice_step(hi - lo)
        t = math.ceil(lo / step) * step
        while t <= hi:
            x = sx(t)
            out.append(
                f'<line class="grid" x1="{x:.1f}" y1="{top - 6}" x2="{x:.1f}" y2="{y_end}" '
                'stroke-width="1"/>'
            )
            out.append(
                f'<text class="muted" x="{x:.1f}" y="{y_end + 16}" text-anchor="middle">'
                f"{format(t, fmt) if abs(t) > 1e-9 else '0'}</text>"
            )
            t += step
        xn = sx(null)
        out.append(
            f'<line class="ref" x1="{xn:.1f}" y1="{top - 10}" x2="{xn:.1f}" y2="{y_end}" '
            'stroke-width="1.5" stroke-dasharray="4 3"/>'
        )
        for i, (est, a, b) in enumerate(vals):
            y = top + row_h * i + row_h / 2
            out.append(
                f'<line class="ci" x1="{sx(a):.1f}" y1="{y}" x2="{sx(b):.1f}" y2="{y}" '
                'stroke-width="2" stroke-linecap="round"/>'
            )
            out.append(f'<circle class="dot" cx="{sx(est):.1f}" cy="{y}" r="5" stroke-width="2"/>')

    panel(label_w, "a win % (95 % CI)", [(r.win, r.win_lo, r.win_hi) for r in rows], 50, ".0f")
    panel(
        label_w + panel_w + gap,
        "a mean spread (95 % CI)",
        [(r.spread, r.spread_lo, r.spread_hi) for r in rows],
        0,
        "+.0f",
    )
    for i, r in enumerate(rows):
        y = top + row_h * i
        out.append(
            f'<text class="ink" x="12" y="{y + row_h / 2 - 2}">{_esc(r.name)}</text>'
            f'<text class="ink2" x="12" y="{y + row_h / 2 + 12}" font-size="10">'
            f"{r.games} games · {_esc(r.significant)}</text>"
        )
        tip = (
            f"{r.name}: {r.a} vs {r.b}, {r.games} games\n"
            f"win {r.win:.1f} % ({r.win_lo:.1f} to {r.win_hi:.1f}), p Holm {_p(r.p_win_holm)}\n"
            f"spread {r.spread:+.1f} ({r.spread_lo:+.1f} to {r.spread_hi:+.1f}), "
            f"p Holm {_p(r.p_spread_holm)}"
        )
        out.append(
            f'<rect class="hit" x="0" y="{y}" width="{width}" height="{row_h}">'
            f"<title>{_esc(tip)}</title></rect>"
        )
    return "\n".join(out + ["</svg>"]) + "\n"


def _nice_step(span: float) -> float:
    raw = span / 5
    mag = 10 ** math.floor(math.log10(raw))
    for m in (1, 2, 5, 10):
        if raw <= m * mag:
            return m * mag
    return 10 * mag


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("results", nargs="+", type=Path)
    parser.add_argument("--out", type=Path, help="directory for tournament.md/.csv/.svg")
    parser.add_argument("--alpha", type=float, default=0.05)
    a = parser.parse_args(argv)
    paths = [p for p in a.results if p.exists()]
    if not paths:
        print("no results files yet")
        return 0
    rows = report(paths, a.alpha)
    md = markdown(rows, a.alpha)
    print(md, end="")
    if a.out:
        a.out.mkdir(parents=True, exist_ok=True)
        (a.out / "tournament.md").write_text(md)
        (a.out / "tournament.csv").write_text(to_csv(rows))
        (a.out / "tournament.svg").write_text(svg(rows))
        print(f"written to {a.out}/tournament.{{md,csv,svg}}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
