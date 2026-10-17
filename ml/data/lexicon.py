"""Lexicon normalization (NWL / CSW).

Official lexicons are licensed (NASPA / Collins) and never redistributed: the repo
only holds this code; word lists live in lexica/, outside git.
"""

from __future__ import annotations

import hashlib
import json
import re
from collections.abc import Iterable
from pathlib import Path

_WORD = re.compile(r"^[A-Z]{2,15}$")


def normalize(lines: Iterable[str]) -> list[str]:
    """Uppercase, A-Z only, length 2-15, deduplicated and sorted.

    Lines may carry definitions or extra columns: only the first token is used.
    """
    words: set[str] = set()
    for line in lines:
        token = line.strip().split(maxsplit=1)[0] if line.strip() else ""
        token = token.upper()
        if _WORD.match(token):
            words.add(token)
    return sorted(words)


def write_lexicon(words: list[str], out_dir: Path, name: str, source: str) -> Path:
    """Writes <name>.txt plus a manifest with a hash for reproducibility."""
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / f"{name}.txt"
    data = ("\n".join(words) + "\n").encode()
    path.write_bytes(data)
    manifest = {
        "name": name,
        "source": source,
        "words": len(words),
        "sha256": hashlib.sha256(data).hexdigest(),
    }
    (out_dir / f"{name}.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return path
