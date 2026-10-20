"""Train/val/test split by player.

Every turn goes to the split of its mover, chosen from a hash of the mover's
pseudonymized id, so no player has turns in two splits and the assignment does
not change as more games are added.
"""

from __future__ import annotations

import hashlib

SPLITS = ("train", "val", "test")


def split_of(player_id: str, val: float = 0.1, test: float = 0.1) -> str:
    x = int.from_bytes(hashlib.sha256(player_id.encode()).digest()[:8], "big") / 2**64
    return "test" if x < test else "val" if x < test + val else "train"
