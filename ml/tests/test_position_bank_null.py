"""Identical bots on the position bank play each pair of games as the same game
with the seats swapped, so every pair splits; skipped without the built engine."""

import re
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
BANK = ROOT / "build" / "engine" / "kingbble_position_bank"
FIXTURES = ROOT / "lexica" / "fixtures"
LINE = re.compile(r"^game (\d+): (\d+) - (\d+), a in seat ([12])")


@pytest.mark.skipif(not BANK.exists(), reason="engine not built")
def test_identical_bots_split_every_pair(tmp_path):
    results = tmp_path / "results.txt"
    subprocess.run(
        [str(BANK), "--lexicon", str(FIXTURES / "enable_small.txt"),
         "--leaves", str(FIXTURES / "leaves_small.klv2"), "--a", "sim",
         "--positions", "2", "--min-bag", "8", "--max-bag", "12",
         "--sim-candidates", "3", "--sim-iterations", "4", "--peg-time", "30",
         "--endgame-time", "30", "--results", str(results)],
        check=True, capture_output=True,
    )  # fmt: skip
    spread = {}
    for line in results.read_text().splitlines():
        g, s0, s1, seat = map(int, LINE.match(line).groups())
        spread[g] = (s0 - s1) if seat == 1 else (s1 - s0)
    assert sorted(spread) == list(range(1, 5))
    for g in range(1, 5, 2):
        assert spread[g] == -spread[g + 1]
