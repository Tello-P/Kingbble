import shutil
import struct

import pytest

from ml.data.features import BASE_FEATURES, DEFAULT_TOOL, ROOT, Turn, build, header, load

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
    rows = np.arange(2 * len(BASE_FEATURES), dtype="<f4").reshape(2, -1)
    (tmp_path / "options.bin").write_bytes(rows.tobytes())
    turns, options = load(tmp_path)
    assert turns == [Turn("g1", 0, 0, "AERTXYZ", 86, 5, 1, 0, 2)]
    assert options.shape == (2, len(BASE_FEATURES))
    assert options[1, 0] == len(BASE_FEATURES)
    # Version 2: the header names the columns.
    names = b"gap opens_tw"
    head = b"KBFT" + struct.pack("<III", 2, 2, len(names)) + names
    (tmp_path / "options.bin").write_bytes(head + rows.tobytes())
    assert header(tmp_path / "options.bin") == (("gap", "opens_tw"), 2, 16 + len(names))
    _, options = load(tmp_path)
    assert options.shape == (8, 2) and options[1, 1] == 3


@pytest.mark.skipif(not DEFAULT_TOOL.exists(), reason="engine not built")
def test_build_on_the_sample_game(tmp_path):
    raw = tmp_path / "raw"
    raw.mkdir()
    shutil.copy(FIXTURES / "sample.gcg", raw)
    build(
        FIXTURES / "enable_small.txt",
        FIXTURES / "leaves_small.klv2",
        raw,
        tmp_path / "f",
        top=8,
        store=None,
        word_freq=None,
        alt_lexicon=None,
    )
    turns, options = load(tmp_path / "f")
    names, version, _ = header(tmp_path / "f" / "options.bin")
    assert version == 2 and names[: len(BASE_FEATURES)] == BASE_FEATURES
    assert "opens_tw" in names and options.shape[1] == len(names)
    # Placements, exchanges and passes with a known rack, the phony skipped.
    assert [t.turn for t in turns] == [0, 1, 2, 3, 5, 6]
    assert sum(t.rows for t in turns) == len(options)
    for t in turns:
        assert options[t.first_row, names.index("gap")] == 0
        if t.chosen >= 0:
            assert t.chosen_row >= 0
