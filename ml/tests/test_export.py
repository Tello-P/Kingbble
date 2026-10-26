import json
import struct

import pytest

from ml.data.export import export, position, tiles, word
from ml.data.store import build

pytest.importorskip("pyarrow")
pytest.importorskip("duckdb")

GAME = """#player1 ann Ann
#player2 bob Bob
#lexicon NWL23
>ann: AERTXY? 8E RATEs +8 8
>bob: QUIZZES 9E (A)H +5 5
>ann: AXY? -AXY +0 8
>bob: QUIZZES - +0 5
"""


def test_encodings():
    assert tiles("AZ?", 5) == [1, 26, 27, 0, 0]
    assert tiles("a.", 2) == [33, 63]
    assert position("8E") == [7, 4, 0] and position("E8") == [7, 4, 1]
    assert word("(A)H") == ".H"


def test_export_writes_one_row_per_human_turn_with_rack(tmp_path):
    raw, store, out = tmp_path / "raw", tmp_path / "turns", tmp_path / "mm"
    raw.mkdir()
    (raw / "g1.gcg").write_text(GAME)
    build(raw, store, salt="salt")
    counts = export(store, out)
    assert sum(counts.values()) == 4
    meta = json.loads((out / "meta.json").read_text())
    for _split, info in meta["splits"].items():
        n = info["turns"]
        for spec in info["arrays"].values():
            size = int(spec["dtype"][-1])
            assert (out / spec["file"]).stat().st_size == n * spec["shape"][1] * size
    # ann's turns sit together in one split, in turn order.
    rows = []
    for _split, info in meta["splits"].items():
        rack = (out / info["arrays"]["rack"]["file"]).read_bytes()
        nums = (out / info["arrays"]["numbers"]["file"]).read_bytes()
        for i in range(info["turns"]):
            r = "".join(chr(c + 64) if c < 27 else "?" for c in rack[7 * i : 7 * i + 7] if c)
            rows.append((r, struct.unpack_from("<6h", nums, 12 * i)))
    assert ("AERTXY?", (8, 0, 0, 86, -1, 0)) in rows
    assert ("AXY?", (0, 8, 5, 80, -1, 2)) in rows
