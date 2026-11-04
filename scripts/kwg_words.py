#!/usr/bin/env python3
"""Writes the word list stored in a KWG (wolges / Macondo format), one word per line.

The strength runs read a word list. A CI runner only has the public KWG, so it
rebuilds the list there; the result stays on the runner (lexica/ is ignored by git).
Published KWGs may hold words too long for the board: they are left out by default.

  python scripts/kwg_words.py lexica/CSW24.kwg lexica/CSW24.txt
"""

from __future__ import annotations

import argparse
import array
import sys
from pathlib import Path

NO_CHILDREN = 0
CHILDREN_MASK = (1 << 22) - 1
IS_END = 1 << 22
ACCEPTS = 1 << 23
TILE_SHIFT = 24
BOARD_SIZE = 15


def words(arcs: array.array) -> list[str]:
    """The words of the KWG's DAWG, in alphabetical order."""
    found: list[str] = []
    # (first arc of a sibling list, the prefix leading to it)
    stack = [(arcs[0] & CHILDREN_MASK, "")]
    while stack:
        first, prefix = stack.pop()
        if first == NO_CHILDREN:
            continue
        for i in range(first, len(arcs)):
            arc = arcs[i]
            word = prefix + chr(ord("A") + (arc >> TILE_SHIFT) - 1)
            if arc & ACCEPTS:
                found.append(word)
            stack.append((arc & CHILDREN_MASK, word))
            if arc & IS_END:
                break
    return sorted(found)


def load(path: Path) -> array.array:
    arcs = array.array("I")
    arcs.frombytes(path.read_bytes())
    if arcs.itemsize != 4:
        raise SystemExit("needs a 32-bit array type")
    if sys.byteorder == "big":
        arcs.byteswap()
    return arcs


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("kwg", type=Path)
    p.add_argument("out", type=Path)
    p.add_argument("--max-length", type=int, default=BOARD_SIZE)
    args = p.parse_args()
    found = [w for w in words(load(args.kwg)) if len(w) <= args.max_length]
    args.out.write_text("".join(w + "\n" for w in found), encoding="ascii")
    print(f"{len(found)} words -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
