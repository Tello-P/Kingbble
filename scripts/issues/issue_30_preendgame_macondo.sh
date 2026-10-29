#!/usr/bin/env bash
# #30 (also #32 part 2, #20, #26-#28): pre-endgames against Macondo with passing,
# 1-tile and 2-tile bags. Check: 0 mismatched positions in both runs; the wall
# times are printed at the end of each run.
# --quick: 2 positions per job with 1 tile in the bag only. Every position is
# capped at LIMIT s per side (default 90; slower ones are skipped and listed;
# another LIMIT keeps its results apart, e.g. LIMIT=600 ...).
# Every finished game/position is kept: stop the script any time and run it again
# to go on (--fresh starts over, --status reports). JOBS (default MAX_THREADS) sets
# the parallel jobs, which also pick the positions: resume with the same JOBS.
#   scripts/issues/issue_30_preendgame_macondo.sh [--dry-run] [--quick] [--fresh] [--status]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
COUNT=4 JOBS=${JOBS:-$MAX_THREADS} BAGS="1 2" LIMIT=${LIMIT:-90}
((QUICK)) && COUNT=2 BAGS=1
RUN_UNITS=$((2 * COUNT * JOBS))  # each position is solved by us and by Macondo
issue_init 30 $((RUN_UNITS * $(wc -w <<<"$BAGS"))) "$@"
need "$HOME/src/macondo" "clone https://github.com/domino14/macondo to ~/src/macondo"
build_engine

for bag in $BAGS; do
  note "max-bag $bag"
  step $RUN_UNITS "" "$PY" -u scripts/compare_preendgame.py --count $COUNT --jobs $JOBS \
    --max-bag $bag --candidates 3 --pass --time-limit $LIMIT --build "$ROOT/build" \
    --work "$ROOT/build/macondo_compare" --results "$(results_file bag$bag$( ((LIMIT != 90)) && echo _t$LIMIT))"
done
finish || grep -E "mismatched positions|wall time" "$LOG" | tail -4
