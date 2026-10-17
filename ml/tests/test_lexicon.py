import json
from pathlib import Path

from ml.data.lexicon import normalize, write_lexicon

FIXTURE = Path(__file__).resolve().parents[2] / "lexica" / "fixtures" / "tiny_raw.txt"


def test_normalize_uppercases_dedupes_and_sorts():
    words = normalize(FIXTURE.read_text().splitlines())
    assert words == ["AA", "AB", "ACT", "CAT", "CATS", "HELLO", "QI", "TAC", "ZA"]


def test_normalize_drops_invalid_and_keeps_first_token():
    lines = ["A", "ÑU", "x1", "", "  ", "QUIXOTRY n. an impractical idea", "ABCDEFGHIJKLMNOP"]
    assert normalize(lines) == ["QUIXOTRY"]


def test_write_lexicon_manifest(tmp_path):
    path = write_lexicon(["AA", "QI"], tmp_path, "TEST", "fixture")
    assert path.read_text() == "AA\nQI\n"
    manifest = json.loads((tmp_path / "TEST.json").read_text())
    assert manifest["words"] == 2
    assert len(manifest["sha256"]) == 64
