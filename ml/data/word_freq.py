"""Word familiarity table for the move features: English frequency plus play counts.

  python -m ml.data.word_freq [--lexicon lexica/CSW24.txt] [--raw data/raw/woogles]
                              [--store data/turns] [--out lexica/word_freq.tsv]

For every word of the lexicon, its Zipf frequency in English from wordfreq
(code Apache-2.0, data CC BY-SA 4.0: cite it) and the number of times humans
played it as a main word in the train split of our games (counted by
kingbble_features --count-words, so val and test turns do not leak). Words with
neither are left out. Output: "word<TAB>zipf<TAB>plays" with a header line.
"""

from __future__ import annotations

import argparse
import subprocess
import tempfile
from pathlib import Path

from ml.data.features import DEFAULT_TOOL, DEFAULT_WORD_FREQ, ROOT, store_turns, write_turn_info
from ml.data.store import DEFAULT_STORE
from ml.data.woogles import DEFAULT_OUT as DEFAULT_RAW

DEFAULT_LEXICON = ROOT / "lexica" / "CSW24.txt"


def read_words(path: Path) -> list[str]:
    words = set()
    for line in path.read_text().splitlines():
        parts = line.split()
        if parts:
            words.add(parts[0].upper())
    return sorted(words)


def play_counts(lexicon: Path, raw: Path, store: Path, tool: Path = DEFAULT_TOOL) -> dict:
    with tempfile.TemporaryDirectory() as tmp:
        info, out = Path(tmp) / "turn_info.tsv", Path(tmp) / "counts.tsv"
        write_turn_info(store_turns(store), info)
        subprocess.run([str(tool), "--lexicon", str(lexicon), "--games", str(raw),
                        "--turn-info", str(info), "--count-words", str(out)],
                       check=True, stdout=subprocess.DEVNULL)
        counts = {}
        for line in out.read_text().splitlines():
            word, n = line.split("\t")
            counts[word] = int(n)
    return counts


def table(words, counts: dict, zipf) -> list[tuple[str, float, int]]:
    """(word, zipf, plays) for the words with either; zipf maps a word to its frequency."""
    rows = []
    for w in words:
        z, n = zipf(w), counts.get(w, 0)
        if z > 0 or n > 0:
            rows.append((w, z, n))
    return rows


def write(rows, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        f.write("word\tzipf\tplays\n")
        for w, z, n in rows:
            f.write(f"{w}\t{z:.2f}\t{n}\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--lexicon", type=Path, default=DEFAULT_LEXICON)
    ap.add_argument("--raw", type=Path, default=DEFAULT_RAW)
    ap.add_argument("--store", type=Path, default=DEFAULT_STORE)
    ap.add_argument("--out", type=Path, default=DEFAULT_WORD_FREQ)
    ap.add_argument("--tool", type=Path, default=DEFAULT_TOOL)
    args = ap.parse_args()
    try:
        from wordfreq import zipf_frequency
    except ImportError:
        raise SystemExit("wordfreq is missing: pip install -e '.[freq]'") from None
    counts = play_counts(args.lexicon, args.raw, args.store, args.tool)
    rows = table(read_words(args.lexicon), counts, lambda w: zipf_frequency(w.lower(), "en"))
    write(rows, args.out)
    played = sum(1 for _, _, n in rows if n > 0)
    print(f"{len(rows)} words ({played} played in train games), written to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
