"""Stable pseudonyms for player nicknames.

A pseudonym is a salted SHA-256 of the nickname, so the same player gets the
same id in every build while the store holds no names. The salt is secret and
never in the repo: $KINGBBLE_SALT, else data/salt (created on first use).
"""

from __future__ import annotations

import hashlib
import os
import secrets
from pathlib import Path

DEFAULT_SALT_FILE = Path(__file__).resolve().parents[2] / "data" / "salt"


def load_salt(path: Path = DEFAULT_SALT_FILE) -> str:
    salt = os.environ.get("KINGBBLE_SALT")
    if salt:
        return salt
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(secrets.token_hex(32) + "\n")
        path.chmod(0o600)
    return path.read_text().strip()


def pseudonym(nickname: str, salt: str) -> str:
    """'p' + 16 hex digits; nicknames are compared case-insensitively, as on Woogles."""
    digest = hashlib.sha256(f"{salt}\0{nickname.lower()}".encode()).hexdigest()
    return "p" + digest[:16]
