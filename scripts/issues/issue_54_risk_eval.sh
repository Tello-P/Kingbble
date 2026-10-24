#!/usr/bin/env bash
# #54: RiskBot against the M4 SimBot on CSW24 (M5 exit criterion, tag v0.5).
# Two runs, risk (mean - lambda * variance) vs sim and riskwin (max win %) vs sim,
# GAMES games each, paired: every deal is played twice with the bots swapping
# seats. Check: a win % 95 % CI above 50 for at least one mode (CI over the pair
# means, ties count half).
#
# Full mode: 800 games per run (enough for +5 points of win % at 95 % with 80 %
# power), 10 candidates x 100 iterations, about 15 s per game on 3 threads:
# about 6 h 40 in all. --quick: 40 games per run, 8 candidates x 50 iterations,
# about 6.5 s per game: about 9 min. Not the final validation.
# Every finished game is kept: stop the script any time and run it again to go
# on (--fresh starts over, --status reports).
# Resources: --threads N (or KINGBBLE_THREADS, default 3) and --mem SIZE (or
# KINGBBLE_MEM_MAX, default 6G) under lib.sh's LIMITS. It refuses to start while
# another issue script or engine binary runs, unless --force.
#   scripts/issues/issue_54_risk_eval.sh [--quick] [--dry-run] [--fresh] [--status]
#     [--games N] [--lambda L] [--modes "risk riskwin"] [--threads N] [--mem SIZE] [--force]
# risk's results are kept per --lambda (riskwin does not use it), so a lambda
# sweep is one run per value with --modes risk.
source "$(dirname "$0")/lib.sh"

quick_args "$@"
GAMES=800 LAMBDA=1 GAME_UNITS=15 MODES=(risk riskwin)
SIM=(--sim-candidates 10 --sim-iterations 100)
if ((QUICK)); then
  GAMES=40 GAME_UNITS=7
  SIM=(--sim-candidates 8 --sim-iterations 50)
fi
args=("$@")
for ((i = 0; i < ${#args[@]}; i++)); do
  case ${args[i]} in
    --games) GAMES=${args[i + 1]} ;;
    --lambda) LAMBDA=${args[i + 1]} ;;
    --modes) read -ra MODES <<<"${args[i + 1]}" ;;
  esac
done
# Whole pairs only.
GAMES=$((GAMES + GAMES % 2))
# Results file name of a mode's run.
run_name() { if [[ $1 == risk ]]; then echo "risk_l$LAMBDA"; else echo "$1"; fi; }

limits_init "$@"
issue_init 54 $((${#MODES[@]} * GAMES * GAME_UNITS)) "$@"
((STATUS || DRY_RUN)) || busy_check 54
need_csw24
build_engine

for mode in "${MODES[@]}"; do
  STEP_SCALE=$GAME_UNITS step "$GAMES" "" build/engine/kingbble_selfplay \
    --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --a "$mode" --b sim --paired \
    --games "$GAMES" --seed 1 "${SIM[@]}" --threads "$MAX_THREADS" --risk-lambda "$LAMBDA" \
    --progress --results "$(results_file "$(run_name "$mode")")"
done
((DRY_RUN)) && exit 0
# Win % with a 95 % CI over the finished pairs ("game N: S1 - S2, a in seat K").
for mode in "${MODES[@]}"; do
  f=$(results_file "$(run_name "$mode")")
  [[ -e $f ]] || continue
  awk -v mode="$(run_name "$mode")" -v games="$GAMES" '
    match($0, /^game ([0-9]+): (-?[0-9]+) - (-?[0-9]+), a in seat ([12])/, m) {
      g = m[1]
      if (g > games) next
      a = m[4] == 1 ? m[2] : m[3]
      b = m[4] == 1 ? m[3] : m[2]
      score[g] = a > b ? 1 : a < b ? 0 : 0.5
      spread[g] = a - b
    }
    END {
      for (g in score) {
        if (g % 2 == 0 || !((g + 1) in score)) continue
        p = (score[g] + score[g + 1]) / 2
        n++; sum += p; sq += p * p
        sp += spread[g] + spread[g + 1]
      }
      if (n < 2) { printf "[#54] %s vs sim: %d pairs, too few\n", mode, n; exit }
      mean = sum / n
      v = (sq - n * mean * mean) / (n - 1) / n
      se = v > 0 ? sqrt(v) : 0
      lo = 100 * (mean - 1.96 * se); hi = 100 * (mean + 1.96 * se)
      printf "[#54] %s vs sim: %d games, a win %% %.1f (95%% CI %.1f-%.1f), a avg spread %+.1f, %s\n",
        mode, 2 * n, 100 * mean, lo, hi, sp / (2 * n), (lo > 50 ? "significant gain" : "no significant gain")
    }' "$f" | if ((STATUS)); then cat; else tee -a "$LOG"; fi
done
