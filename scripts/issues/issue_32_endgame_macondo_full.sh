#!/usr/bin/env bash
# #32 part 1 (and #23, #26): 50 full-size endgames (up to 14 tiles) against
# Macondo on 8 jobs. Check: "mismatched values: 0"; write down both run times.
# Part 2 of #32 is the #30 run: issue_30_preendgame_macondo.sh.
# --quick: 50 other positions (seed 2) of up to 8 tiles, each capped at 10 s per
# side (slower ones are skipped and listed).
#   scripts/issues/issue_32_endgame_macondo_full.sh [--dry-run] [--quick]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
COUNT=50 TILES=14 SEED=1 LIMIT=0
((QUICK)) && TILES=8 SEED=2 LIMIT=10
issue_init 32 $((2 * COUNT)) "$@"
need "$HOME/src/macondo" "clone https://github.com/domino14/macondo to ~/src/macondo"
build_engine

step $((2 * COUNT)) "" "$PY" -u scripts/compare_endgame.py --macondo "$HOME/src/macondo" \
  --count $COUNT --max-tiles $TILES --seed $SEED --jobs 8 --time-limit $LIMIT \
  --build "$ROOT/build" --work "$ROOT/build/macondo_compare"
