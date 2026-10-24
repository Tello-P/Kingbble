#!/usr/bin/env bash
# #54: RiskBot against SimBot from a bank of mid and late game positions (bag
# 10-25, lead within +-80) on CSW24, both with the pre-endgame and endgame
# solvers and a rack posterior (RangeFinder) tracked from the start of the game.
# Each position is played to the end twice per repeat, the risk bot and then
# the sim bot on the side to move. Two runs: risk and riskwin.
# Check: a win % 95 % CI above 50 (CI over the pair means, ties count half).
#
# Full mode: 100 positions x 2 repeats (400 games per run), 10 candidates x 100
# iterations, pre-endgame 5 s and endgame 2 s: about 20 s per game on 3 threads,
# about 4 h 30 in all.
# --quick: 30 positions x 1 repeat (60 games per run), 8 x 50, 2 s and 1 s:
# about 5 s per game, about 10 min in all. Not the final validation.
# Every finished game is kept: stop the script any time and run it again to go
# on (--fresh starts over, --status reports).
# Resources: --threads N (or KINGBBLE_THREADS, default 3) and --mem SIZE (or
# KINGBBLE_MEM_MAX, default 6G) under lib.sh's LIMITS. It refuses to start while
# another issue script or engine binary runs, unless --force.
#   scripts/issues/issue_54_position_bank.sh [--quick] [--dry-run] [--fresh] [--status]
#     [--positions N] [--repeats N] [--lambda L] [--modes "risk riskwin"] [--threads N]
#     [--mem SIZE] [--force]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
POSITIONS=100 REPEATS=2 LAMBDA=1 GAME_UNITS=20 MODES=(risk riskwin)
SIM=(--sim-candidates 10 --sim-iterations 100 --peg-time 5 --endgame-time 2)
if ((QUICK)); then
  POSITIONS=30 REPEATS=1 GAME_UNITS=5
  SIM=(--sim-candidates 8 --sim-iterations 50 --peg-time 2 --endgame-time 1)
fi
args=("$@")
for ((i = 0; i < ${#args[@]}; i++)); do
  case ${args[i]} in
    --positions) POSITIONS=${args[i + 1]} ;;
    --repeats) REPEATS=${args[i + 1]} ;;
    --lambda) LAMBDA=${args[i + 1]} ;;
    --modes) read -ra MODES <<<"${args[i + 1]}" ;;
  esac
done
GAMES=$((2 * POSITIONS * REPEATS))
# Results file name of a mode's run.
run_name() { if [[ $1 == risk ]]; then echo "bank_risk_l$LAMBDA"; else echo "bank_$1"; fi; }

limits_init "$@"
issue_init 54 $((${#MODES[@]} * GAMES * GAME_UNITS)) "$@"
((STATUS || DRY_RUN)) || busy_check 54
need_csw24
build_engine

for mode in "${MODES[@]}"; do
  STEP_SCALE=$GAME_UNITS step "$GAMES" "" build/engine/kingbble_position_bank \
    --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --a "$mode" \
    --positions "$POSITIONS" --repeats "$REPEATS" --seed 1 "${SIM[@]}" --threads "$MAX_THREADS" \
    --risk-lambda "$LAMBDA" --progress --results "$(results_file "$(run_name "$mode")")"
done
((DRY_RUN)) && exit 0
for mode in "${MODES[@]}"; do
  f=$(results_file "$(run_name "$mode")")
  [[ -e $f ]] || continue
  awk -v mode="$(run_name "$mode")" -v games="$GAMES" -f "$(dirname "$0")/paired_ci.awk" "$f" |
    if ((STATUS)); then cat; else tee -a "$LOG"; fi
done
