#!/usr/bin/env bash
# #66: M6a acceptance. Level 0 of the training menu runs end to end unattended:
# the Auto loop (app.cli auto, stopped after level 0) generates the self-play
# games, fits the move model with its learning curve, gates it with the rack
# log-loss and a paired match, and logs it all to MLflow (build/mlruns). Check:
# the run finishes on its own and the model beats the uniform bag in log-loss.
#
# Full mode: level 0 (1,000 games), log-loss on 300 test turns and a 10-game
# match at 5 candidates x 30 (about 15 min on 2 threads). --quick: 50 games, a 4-game match at
# 3 x 10 (about 3 min). The loop keeps its state in its work directory: stop the
# script any time and run it again to go on (--fresh starts over, --status
# reports).
# Resources: --threads N (or KINGBBLE_THREADS, default 3) and --mem SIZE (or
# KINGBBLE_MEM_MAX, default 6G); every engine and fitting step runs under the
# same limits (ml.limits). It refuses to start while another issue script or
# engine binary runs, unless --force.
#   scripts/issues/issue_66_level0.sh [--quick] [--dry-run] [--fresh] [--status]
#     [--threads N] [--mem SIZE] [--force]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
limits_init "$@"
issue_init 66 1 "$@"
WORK=$LOG_DIR/issue_66$(quick_suffix)
if ((STATUS)); then
  "$PY" -m app.cli status --work "$WORK"
  exit 0
fi
((DRY_RUN)) || busy_check 66
need_csw24
build_engine
if ((FRESH && !DRY_RUN)); then rm -rf "$WORK"; fi

SIZE=(--level 0)
MATCH=(--eval-games 10 --eval-sim-candidates 5 --eval-sim-iterations 30 --eval-max-turns 300)
if ((QUICK)); then
  SIZE=(--games 50)
  MATCH=(--eval-games 4 --eval-sim-candidates 3 --eval-sim-iterations 10 --eval-max-turns 300)
fi
# --force: the busy check above already ran, and this script would count as busy.
step 1 "" "$PY" -m app.cli auto --work "$WORK" "${SIZE[@]}" --max-level 0 \
  --budget-cpu-hours 3 --budget-gb 2 --threads "$MAX_THREADS" --mem "$MAX_MEM" "${MATCH[@]}" \
  --force
finish && exit 0

"$PY" - "$WORK" <<'PY' | tee -a "$LOG"
import json, sys
from pathlib import Path

state = json.loads((Path(sys.argv[1]) / "auto.json").read_text())
# The best version, or the last one evaluated if none was kept.
version = Path(state["best"] or state["history"][-1]["version"])
e = json.loads((version / "evaluation.json").read_text())
ll = e["log_loss"]
ok = state["stopped"] is not None and bool(e["beats_uniform"])
print(f"[#66] {version.name} ({e['verdict']}): log-loss {ll['new']:.4f} vs uniform {ll['none']:.4f} "
      f"on {ll['turns']} test turns, {e['match']['games']} match games, stopped: "
      f"{state['stopped']}: {'PASS' if ok else 'FAIL'}")
sys.exit(0 if ok else 1)
PY
