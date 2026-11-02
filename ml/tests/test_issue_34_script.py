import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "issues" / "issue_34_quackle_and_smp.sh"


def match_line(*args):
    r = subprocess.run(
        ["bash", str(SCRIPT), "--dry-run", "--match-only", *args],
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert r.returncode == 0, r.stderr
    (line,) = [x for x in r.stdout.splitlines() if "kingbble_quackle_match" in x]
    return line


def test_fixed_iterations_by_default():
    line = match_line()
    assert "--sim-time" not in line
    assert line.rstrip().endswith("issue_34_match.txt")


def test_sim_time_has_its_own_results_file():
    line = match_line("--sim-time", "5")
    assert line.rstrip().endswith("--sim-time 5 --sim-iterations 5000")
    assert "issue_34_match_simt5.txt" in line
    assert "quackle_games/match_simt5" in line


def test_sim_time_overrides_quick_iterations():
    line = match_line("--quick", "--sim-time", "2.5", "--sim-cap", "800")
    assert line.index("--sim-iterations 40") < line.index("--sim-iterations 800")
    assert "issue_34_match_simt2.5_quick.txt" in line


def test_rejects_bad_sim_time():
    r = subprocess.run(
        ["bash", str(SCRIPT), "--dry-run", "--sim-time", "fast"],
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert r.returncode == 2
    assert "--sim-time wants seconds" in r.stderr
