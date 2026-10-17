#!/usr/bin/env python3
"""Prepares a lexicon in lexica/ from a local file or a URL.

Examples:
  # Official lexicon you already have (NWL2023 / CSW24), without redistributing it:
  python scripts/fetch_lexicon.py --name NWL2023 --source ~/Descargas/NWL2023.txt

  # Free list (ENABLE, public domain) for development and tests:
  python scripts/fetch_lexicon.py --preset enable
"""

from __future__ import annotations

import argparse
import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ml.data.lexicon import normalize, write_lexicon  # noqa: E402

PRESETS = {
    "enable": ("ENABLE", "https://raw.githubusercontent.com/dolph/dictionary/master/enable1.txt"),
}


def read_source(source: str) -> list[str]:
    if source.startswith(("http://", "https://")):
        with urllib.request.urlopen(source, timeout=60) as resp:
            return resp.read().decode("utf-8", errors="replace").splitlines()
    return Path(source).expanduser().read_text(encoding="utf-8", errors="replace").splitlines()


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("--name", help="Lexicon name (e.g. NWL2023, CSW24)")
    p.add_argument("--source", help="Local path or URL of the word list")
    p.add_argument("--preset", choices=sorted(PRESETS), help="Predefined free word list")
    p.add_argument("--out", default="lexica", help="Output directory (outside git)")
    args = p.parse_args()

    if args.preset:
        name, source = PRESETS[args.preset]
    elif args.name and args.source:
        name, source = args.name, args.source
    else:
        p.error("use --preset or --name and --source")

    words = normalize(read_source(source))
    if not words:
        print(f"error: no valid words in {source}", file=sys.stderr)
        return 1
    path = write_lexicon(words, Path(args.out), name, source)
    print(f"{name}: {len(words)} words -> {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
