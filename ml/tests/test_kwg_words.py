import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("kwg_words", ROOT / "scripts" / "kwg_words.py")
kwg_words = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(kwg_words)

FIXTURES = ROOT / "lexica" / "fixtures"


def test_words_are_the_list_the_kwg_was_built_from():
    expected = (FIXTURES / "enable_small.txt").read_text().split()
    assert kwg_words.words(kwg_words.load(FIXTURES / "enable_small.kwg")) == sorted(
        w.upper() for w in expected
    )


def test_main_leaves_out_words_too_long_for_the_limit(tmp_path, monkeypatch):
    out = tmp_path / "words.txt"
    kwg = FIXTURES / "enable_small.kwg"
    monkeypatch.setattr("sys.argv", ["kwg_words", str(kwg), str(out), "--max-length", "3"])
    assert kwg_words.main() == 0
    words = out.read_text().split()
    assert words and max(len(w) for w in words) <= 3
