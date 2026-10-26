import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "issues" / "issue_47_inference_eval.sh"


def run(*args):
    return subprocess.run(
        ["bash", str(SCRIPT), "--quick", "--dry-run", *args],
        capture_output=True,
        text=True,
        timeout=60,
    )


def test_refuses_while_another_issue_script_runs(tmp_path):
    busy = tmp_path / "issue_99_busy.sh"
    busy.write_text("sleep 30\n")
    proc = subprocess.Popen(["bash", str(busy)])
    try:
        r = run()
        assert r.returncode == 1
        assert f"{proc.pid} bash {busy}" in r.stderr
        assert run("--force").returncode == 0
    finally:
        proc.kill()
        proc.wait()


def test_threads_and_memory_cap_reach_the_heavy_steps():
    r = run("--force", "--threads", "2", "--mem", "none")
    assert r.returncode == 0, r.stderr
    games = [line for line in r.stdout.splitlines() if "--games-played" in line]
    assert len(games) == 4
    assert all("--threads 2" in line for line in games)
    assert "systemd-run" not in r.stdout
    assert "cmake --build build -j2" in r.stdout
