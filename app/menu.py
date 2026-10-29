"""Streamlit interface for the training menu (app.cli).

  streamlit run app/menu.py            # the optional "app" extra

The sidebar sets what app.cli takes (work directory, source, level or games,
rating range, human/self-play mix, k, threads, Auto budget); the buttons start
train, evaluate, improve or the Auto loop as a background `python -m app.cli`
process (one at a time, under ml.limits like the CLI), whose log the page shows.
The page lists the versions with their learning curves and evaluations, and the
Auto loop's state. The page logic is plain functions (tested without Streamlit);
only render() imports streamlit.
"""

from __future__ import annotations

import json
import os
import signal
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK = ROOT / "build" / "m6"
COMMANDS = ("train", "evaluate", "improve", "auto")


def cli_args(command: str, s: dict) -> list[str]:
    """The app.cli arguments for `command` from the sidebar settings `s`."""
    args = [command, "--work", str(s["work"]), "--threads", str(s["threads"]), "--k", str(s["k"])]
    if command in ("train", "auto", "improve"):
        args += ["--source", s["source"]]
        args += ["--games", str(s["games"])] if s.get("games") else ["--level", str(s["level"])]
        if s.get("min_rating"):
            args += ["--min-rating", str(s["min_rating"])]
        if s.get("max_rating"):
            args += ["--max-rating", str(s["max_rating"])]
        if s.get("mix"):
            args += ["--mix", str(s["mix"]), "--mix-raw", str(s["mix_raw"])]
    if command == "auto":
        args += ["--budget-cpu-hours", str(s["budget_cpu_hours"])]
        args += ["--budget-gb", str(s["budget_gb"]), "--max-level", str(s["max_level"])]
    return args


def job_files(work: Path) -> tuple[Path, Path]:
    """(pid file, log file) of the menu's background job."""
    return work / "menu.pid", work / "menu.log"


def running(work: Path) -> int | None:
    """The background job's pid if it is still running."""
    pid_file, _ = job_files(work)
    try:
        pid = int(pid_file.read_text().split()[0])
        os.kill(pid, 0)
    except (OSError, ValueError, IndexError):
        return None
    return pid


def start(command: str, s: dict) -> int:
    """Starts `python -m app.cli <command> ...` in the background; returns its pid."""
    work = Path(s["work"])
    work.mkdir(parents=True, exist_ok=True)
    pid_file, log_file = job_files(work)
    with open(log_file, "a") as log:
        log.write(f"$ app.cli {' '.join(cli_args(command, s))}\n")
        log.flush()
        proc = subprocess.Popen(
            [sys.executable, "-u", "-m", "app.cli", *cli_args(command, s)],
            cwd=ROOT,
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
    pid_file.write_text(f"{proc.pid} {command}\n")
    return proc.pid


def stop(work: Path) -> bool:
    """Stops the background job (its whole process group); False if none runs."""
    pid = running(work)
    if pid is None:
        return False
    os.killpg(pid, signal.SIGTERM)
    return True


def tail(path: Path, lines: int = 40) -> str:
    if not path.exists():
        return ""
    return "\n".join(path.read_text(errors="replace").splitlines()[-lines:])


def versions(work: Path) -> list[dict]:
    """One row per trained version: level, games, curve, evaluation."""
    rows = []
    for v in sorted((work / "models").glob("v[0-9][0-9][0-9]")):
        if not (v / "config.json").exists():
            continue
        config = json.loads((v / "config.json").read_text())
        curve = json.loads((v / "curve.json").read_text()) if (v / "curve.json").exists() else []
        row = {
            "version": v.name,
            "level": config.get("level"),
            "source": config.get("source", "selfplay"),
            "games": config.get("games_played"),
            "top_k": config.get("top_k"),
            "val_nll": curve[-1]["val_nll"] if curve else None,
            "test_nll": config.get("test_nll_mlp", config.get("test_nll_logit")),
            "curve": curve,
            "verdict": None,
        }
        if (v / "evaluation.json").exists():
            e = json.loads((v / "evaluation.json").read_text())
            row["verdict"] = e.get("verdict")
            row["log_loss"] = e.get("log_loss", {}).get("new")
        rows.append(row)
    return rows


def auto_state(work: Path) -> dict | None:
    path = work / "auto.json"
    return json.loads(path.read_text()) if path.exists() else None


def render() -> None:
    import streamlit as st

    st.set_page_config(page_title="Kingbble training menu", layout="wide")
    st.title("Kingbble training menu")
    with st.sidebar:
        s = {
            "work": st.text_input("Work directory", str(DEFAULT_WORK)),
            "source": st.selectbox("Data", ["human", "selfplay"]),
            "level": st.selectbox("Level", [0, 1, 2, 3]),
            "games": st.number_input("Custom games (0: the level's)", 0, 10**7, 0, step=1000),
            "min_rating": st.number_input("Min rating (0: any)", 0, 3000, 0, step=100),
            "max_rating": st.number_input("Max rating (0: any)", 0, 3000, 0, step=100),
            "mix": st.slider("Self-play share of the train turns", 0.0, 0.9, 0.0, 0.05),
            "mix_raw": st.text_input("Self-play GCG directory (for the mix)", ""),
            "k": st.text_input("k (candidates per turn, or auto)", "64"),
            "threads": st.selectbox("Cores", [2, 3], index=0),
            "budget_cpu_hours": st.number_input("Auto: CPU hours", 0.1, 500.0, 3.0),
            "budget_gb": st.number_input("Auto: disk GB", 0.1, 1000.0, 20.0),
            "max_level": st.selectbox("Auto: last level", [0, 1, 2, 3], index=1),
        }
    work = Path(s["work"])
    pid = running(work)
    cols = st.columns(len(COMMANDS) + 1)
    for col, command in zip(cols, COMMANDS, strict=False):
        if col.button(command.capitalize(), disabled=pid is not None):
            start(command, s)
            st.rerun()
    if cols[-1].button("Stop", disabled=pid is None):
        stop(work)
        st.rerun()
    st.caption(f"running: pid {pid}" if pid else "no job running")
    st.code(tail(job_files(work)[1]) or "(no log yet)")

    rows = versions(work)
    if rows:
        st.subheader("Versions")
        st.dataframe([{k: v for k, v in r.items() if k != "curve"} for r in rows])
        chosen = st.selectbox(
            "Learning curve of", [r["version"] for r in rows], index=len(rows) - 1
        )
        curve = next(r["curve"] for r in rows if r["version"] == chosen)
        if curve:
            st.line_chart({"train turns": [c["train_turns"] for c in curve],
                           "val NLL": [c["val_nll"] for c in curve]},
                          x="train turns", y="val NLL")  # fmt: skip
    state = auto_state(work)
    if state:
        st.subheader("Auto loop")
        st.json(state)


if __name__ == "__main__":
    render()
