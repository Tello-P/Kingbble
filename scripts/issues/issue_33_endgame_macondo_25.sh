#!/usr/bin/env bash
# #33 (and #25): 140 endgames of up to 8 tiles against Macondo on MAX_THREADS jobs, for the
# #25 speed-up. Check: "mismatched values: 0". Each position is capped at 60 s
# per side; slower ones are skipped and listed (#142 never finishes).
# --quick: 50 positions, each capped at 10 s per side (slower ones are skipped and
# listed).
# Every finished game/position is kept: stop the script any time and run it again
# to go on (--fresh starts over, --status reports).
#   scripts/issues/issue_33_endgame_macondo_25.sh [--dry-run] [--quick] [--fresh] [--status]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
COUNT=140 LIMIT=60
((QUICK)) && COUNT=50 LIMIT=10
issue_init 33 $((2 * COUNT)) "$@"
need "$HOME/src/macondo" "clone https://github.com/domino14/macondo to ~/src/macondo"
build_engine

step $((2 * COUNT)) "" "$PY" -u scripts/compare_endgame.py --count $COUNT --max-tiles 8 --jobs $MAX_THREADS \
  --time-limit $LIMIT --build "$ROOT/build" --work "$ROOT/build/macondo_compare" \
  --results "$(results_file positions)"
