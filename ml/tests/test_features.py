import shutil

import pytest

from ml.data.features import DEFAULT_TOOL, FEATURES, ROOT, Turn, build, load

np = pytest.importorskip("numpy")
FIXTURES = ROOT / "lexica" / "fixtures"


def test_chosen_row_and_top():
    inside = Turn("g", 0, 0, "AB", 80, 100, 3, 0, 8)
    below = Turn("g", 1, 0, "AB", 80, 100, 40, 8, 9)
    missing = Turn("g", 2, 0, "AB", 80, 100, -1, 17, 8)
    assert (inside.chosen_row, inside.top) == (3, 8)
    assert (below.chosen_row, below.top) == (8, 8)
    assert (missing.chosen_row, missing.top) == (-1, 8)


def test_load_reads_turns_and_rows(tmp_path):
    (tmp_path / "turns.tsv").write_text(
        "game_id\tturn\tplayer\track\tbag\toptions\tchosen\tfirst_row\trows\n"
        "g1\t0\t0\tAERTXYZ\t86\t5\t1\t0\t2\n"
    )
    rows = np.arange(2 * len(FEATURES), dtype="<f4").reshape(2, -1)
    (tmp_path / "options.bin").write_bytes(rows.tobytes())
    turns, options = load(tmp_path)
    assert turns == [Turn("g1", 0, 0, "AERTXYZ", 86, 5, 1, 0, 2)]
    assert options.shape == (2, len(FEATURES))
    assert options[1, 0] == len(FEATURES)


@pytest.mark.skipif(not DEFAULT_TOOL.exists(), reason="engine not built")
def test_build_on_the_sample_game(tmp_path):
    raw = tmp_path / "raw"
    raw.mkdir()
    shutil.copy(FIXTURES / "sample.gcg", raw)
    build(FIXTURES / "enable_small.txt", FIXTURES / "leaves_small.klv2", raw, tmp_path / "f", top=8)
    turns, options = load(tmp_path / "f")
    # Placements, exchanges and passes with a known rack, the phony skipped.
    assert [t.turn for t in turns] == [0, 1, 2, 3, 5, 6]
    assert sum(t.rows for t in turns) == len(options)
    for t in turns:
        assert options[t.first_row, FEATURES.index("gap")] == 0
        if t.chosen >= 0:
            assert t.chosen_row >= 0
