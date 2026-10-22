"""Results of a long run, one JSON line per finished item, kept across restarts.

Each line is appended and synced to disk as soon as its item finishes, so a run
stopped at any moment loses at most the items in progress; a rerun reads the
file and skips what is already there. A line cut short by the stop is dropped.
"""

from __future__ import annotations

import json
import os
import threading
from pathlib import Path


class ResultsLog:
    def __init__(self, path: Path | None, read_only: bool = False):
        self.path = path
        self.records: list[dict] = []
        self._lock = threading.Lock()
        if path is None or not path.exists():
            return
        data = path.read_bytes()
        keep = data.rfind(b"\n") + 1  # 0 when there is no complete line
        if keep < len(data) and not read_only:
            with path.open("r+b") as f:
                f.truncate(keep)
        for line in data[:keep].decode().splitlines():
            if line.strip():
                self.records.append(json.loads(line))

    def append(self, record: dict) -> None:
        with self._lock:
            self.records.append(record)
            if self.path is None:
                return
            self.path.parent.mkdir(parents=True, exist_ok=True)
            with self.path.open("a") as f:
                f.write(json.dumps(record) + "\n")
                f.flush()
                os.fsync(f.fileno())
