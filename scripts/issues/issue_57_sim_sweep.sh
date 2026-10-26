#!/usr/bin/env bash
# #57: how much does more simulation buy? On the #54 position bank (mid and late
# game positions, both bots with +peg+eg and a RangeFinder rack posterior, every
# position played twice with the bots swapping sides), SimBot with more
# candidates or iterations plays SimBot with the base settings, 8 candidates x 50
# iterations. Settings (candidates x iterations): 8x200, 8x800, 15x50, 15x200.
# Reports each setting's win % and spread with 95 % CIs (over the pair means)
# and the seconds per move of both bots.
#
# --quick: 30 positions (60 games per setting), about 35 min in all on 3 threads.
# Full mode: 100 positions (200 games per setting), about 2 h. Every finished
# game is kept: stop the script any time and run it again to go on (--fresh
# starts over, --status reports).
# Resources: --threads N (or KINGBBLE_THREADS, default 3) and --mem SIZE (or
# KINGBBLE_MEM_MAX, default 6G) under lib.sh's LIMITS. It refuses to start while
# another issue script or engine binary runs, unless --force.
#   scripts/issues/issue_57_sim_sweep.sh [--quick] [--dry-run] [--fresh] [--status]
#     [--positions N] [--settings "8x200 8x800 15x50 15x200"] [--threads N] [--mem SIZE]
#     [--force]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
POSITIONS=100 SETTINGS=(8x200 8x800 15x50 15x200)
((QUICK)) && POSITIONS=30
args=("$@")
for ((i = 0; i < ${#args[@]}; i++)); do
  case ${args[i]} in
    --positions) POSITIONS=${args[i + 1]} ;;
    --settings) read -ra SETTINGS <<<"${args[i + 1]}" ;;
  esac
done
GAMES=$((2 * POSITIONS))
# Measured seconds per bank game against the 8x50 base, on 3 threads.
units() {
  case $1 in
    8x200) echo 6 ;;
    8x800) echo 15 ;;
    15x50) echo 5 ;;
    *) echo 8 ;;
  esac
}
TOTAL=0
for s in "${SETTINGS[@]}"; do TOTAL=$((TOTAL + GAMES * $(units "$s"))); done

limits_init "$@"
issue_init 57 "$TOTAL" "$@"
((STATUS || DRY_RUN)) || busy_check 57
need_csw24
build_engine

for s in "${SETTINGS[@]}"; do
  STEP_SCALE=$(units "$s") step "$GAMES" "" build/engine/kingbble_position_bank \
    --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --a sim \
    --a-candidates "${s%x*}" --a-iterations "${s#*x}" \
    --positions "$POSITIONS" --repeats 1 --seed 1 --sim-candidates 8 --sim-iterations 50 \
    --peg-time 2 --endgame-time 1 --threads "$MAX_THREADS" \
    --progress --results "$(results_file "sim_$s")"
done
((DRY_RUN)) && exit 0
for s in "${SETTINGS[@]}"; do
  f=$(results_file "sim_$s")
  [[ -e $f ]] || continue
  {
    awk -v mode="sim $s" -v games="$GAMES" -f "$(dirname "$0")/paired_ci.awk" "$f" |
      sed 's/^\[#54\]/[#57]/'
    # Seconds per move of each bot over the finished games.
    awk -v s="$s" '
      match($0, /, a ([0-9]+) moves ([0-9.]+) s, b ([0-9]+) moves ([0-9.]+) s/, m) {
        ma += m[1]; ta += m[2]; mb += m[3]; tb += m[4]
      }
      END { if (ma && mb) printf "[#57] sim %s: s/move %.2f (base 8x50: %.2f)\n", s, ta / ma, tb / mb }
    ' "$f"
  } | if ((STATUS)); then cat; else tee -a "$LOG"; fi
done
