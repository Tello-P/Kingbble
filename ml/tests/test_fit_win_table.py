import importlib.util
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("fit_win_table", ROOT / "scripts/fit_win_table.py")
fit = importlib.util.module_from_spec(SPEC)
sys.modules["fit_win_table"] = fit
SPEC.loader.exec_module(fit)


def counts(text):
    table = defaultdict(lambda: defaultdict(lambda: [0.0, 0]))
    fit.add(table, text)
    return table


def test_rising_pools_what_goes_down():
    assert fit.rising([0.2, 0.6, 0.4, 0.8], [1, 1, 1, 1]) == [0.2, 0.5, 0.5, 0.8]


def test_row_grows_with_the_spread_and_stays_inside_zero_and_one():
    row = fit.fit_row({-50: [100, 1000], 0: [600, 1000], 50: [400, 1000], 100: [950, 1000]})
    assert len(row) == len(fit.SPREADS)
    assert all(a <= b for a, b in zip(row, row[1:], strict=False))
    assert 0 < row[0] and row[-1] < 1


def test_counts_of_several_files_add_up():
    table = counts("10 5 3 4\n10 5 1.5 4\n")
    assert table[10][5] == [4.5, 8]


def test_tile_counts_without_data_take_the_nearest():
    table = counts("\n".join(f"20 {s} {0.5 * 100} 100" for s in fit.SPREADS))
    fitted = fit.fit(table)
    assert set(fitted) == set(range(fit.MAX_UNSEEN + 1))
    assert fitted[7] == fitted[20] == fitted[90]
