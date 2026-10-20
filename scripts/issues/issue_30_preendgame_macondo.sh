#!/usr/bin/env bash
# #30 (also #32 part 2, #20, #26-#28): pre-endgames against Macondo with passing,
# 1-tile and 2-tile bags. Check: 0 mismatched positions in both runs; the wall
# times are printed at the end of each run.
#   scripts/issues/issue_30_preendgame_macondo.sh [--dry-run]
source "$(dirname "$0")/lib.sh"

COUNT=4 JOBS=8
RUN_UNITS=$((2 * COUNT * JOBS))  # each position is solved by us and by Macondo
issue_init 30 $((2 * RUN_UNITS)) "$@"
need "$HOME/src/macondo" "clone https://github.com/domino14/macondo to ~/src/macondo"
build_engine

for bag in 1 2; do
  note "max-bag $bag"
  step $RUN_UNITS "" python3 -u scripts/compare_preendgame.py --count $COUNT --jobs $JOBS \
    --max-bag $bag --candidates 3 --pass --build "$ROOT/build" --work "$ROOT/build/macondo_compare"
done
((DRY_RUN)) || grep -E "mismatched positions|wall time" "$LOG" | tail -4
