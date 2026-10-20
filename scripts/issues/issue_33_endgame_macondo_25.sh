#!/usr/bin/env bash
# #33 (and #25): 140 endgames of up to 8 tiles against Macondo on 8 jobs, for the
# #25 speed-up. Check: "mismatched values: 0". Position #142 never finishes,
# hence 140.
#   scripts/issues/issue_33_endgame_macondo_25.sh [--dry-run]
source "$(dirname "$0")/lib.sh"

COUNT=140
issue_init 33 $((2 * COUNT)) "$@"
need "$HOME/src/macondo" "clone https://github.com/domino14/macondo to ~/src/macondo"
build_engine

step $((2 * COUNT)) "" "$PY" -u scripts/compare_endgame.py --count $COUNT --max-tiles 8 --jobs 8 \
  --build "$ROOT/build" --work "$ROOT/build/macondo_compare"
