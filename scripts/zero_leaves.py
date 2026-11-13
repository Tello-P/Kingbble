#!/usr/bin/env python3
"""Every leave of 1 to 6 tiles the English bag allows, each worth 0, as
"LEAVE,value" lines: the starting table for leaves built from scratch."""
import sys

COUNTS = [2, 9, 2, 2, 4, 12, 2, 3, 2, 9, 1, 1, 4, 2, 6, 8, 2, 1, 6, 4, 6, 4, 2, 2, 1, 2, 1]  # ?, A..Z
LETTERS = "?ABCDEFGHIJKLMNOPQRSTUVWXYZ"


def leaves(i, left, cur, out):
    if cur:
        out.write("".join(cur) + ",0\n")
    if left == 0:
        return
    for t in range(i, 27):
        if cur.count(LETTERS[t]) < COUNTS[t]:
            cur.append(LETTERS[t])
            leaves(t, left - 1, cur, out)
            cur.pop()


sys.stdout.write("leave,value\n")
leaves(0, 6, [], sys.stdout)
