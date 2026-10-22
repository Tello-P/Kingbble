#!/usr/bin/env bash
# #34 (and #21, #24):
#  1. Quackle match, sim+peg+eg, 10 seeds x 200 games. Check: combined Kingbble
#     win % >= 45 (v0.2 exit criterion). Finished seeds are skipped on a rerun.
#  2. Lazy SMP endgame: equity+eg vs equity, 200 games with MAX_THREADS and with
#     1 endgame thread. Check: more threads no worse in a win % and a avg spread.
# --quick: 1 x 50 match games and 2 x 20 Lazy SMP games, in separate logs.
#   scripts/issues/issue_34_quackle_and_smp.sh [--dry-run] [--quick] [--match-only | --smp-only]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
SEEDS=10 MATCH_GAMES=200 SMP_GAMES=200 MATCH=match
((QUICK)) && SEEDS=1 MATCH_GAMES=50 SMP_GAMES=20 MATCH=quick_match
issue_init 34 $((SEEDS * MATCH_GAMES + 2 * SMP_GAMES)) "$@"
match=1 smp=1
for a in "${ARGS[@]}"; do
  case $a in
    --match-only) smp=0 ;;
    --smp-only) match=0 ;;
    *) echo "unknown argument: $a" >&2; exit 2 ;;
  esac
done
build_engine

if ((match)); then
  QUACKLE=$HOME/src/quackle
  if ((DRY_RUN)) || [[ ! -x build/quackle_helper/quackle_helper ]]; then
    note "setup: Quackle helper"
    if ((DRY_RUN)); then
      echo "[ -d $QUACKLE ] || git clone https://github.com/quackle/quackle $QUACKLE"
    elif [[ ! -d $QUACKLE ]]; then
      git clone https://github.com/quackle/quackle "$QUACKLE" >>"$LOG" 2>&1
    fi
    step 0 "" cmake -S scripts/quackle_match -B build/quackle_helper -DQUACKLE_DIR="$QUACKLE"
    step 0 "" cmake --build build/quackle_helper -j"$MAX_THREADS"
  fi
  need_csw24
  for seed in $(seq 1 $SEEDS); do
    seed_log=$LOG_DIR/issue_34_${MATCH}_s$seed.log
    if ((!DRY_RUN)) && grep -q "kingbble win %" "$seed_log" 2>/dev/null; then
      note "seed $seed already finished, skipping"
      skip_units $MATCH_GAMES
      continue
    fi
    ((DRY_RUN)) || : >"$seed_log"
    STEP_LOG=$seed_log step $MATCH_GAMES "^game [0-9]+:" build/engine/kingbble_quackle_match \
      --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 \
      --helper build/quackle_helper/quackle_helper --bot sim+peg+eg --threads $MAX_THREADS \
      --quackle-threads $MAX_THREADS \
      --games $MATCH_GAMES --seed "$seed" --gcg-dir "build/quackle_games/${MATCH}_s$seed"
  done
  if ((!DRY_RUN)); then
    # Combined over every finished game of every seed: win % with 95 % CI and mean spread.
    cat "$LOG_DIR"/issue_34_${MATCH}_s*.log | awk '
      /^game [0-9]+: kingbble/ {
        k = $4; q = $7; n++; spread += k - q
        win += (k > q) ? 1 : (k == q) ? 0.5 : 0
      }
      END {
        if (!n) exit
        p = win / n
        printf "[#34] match: %d games, kingbble win %% %.1f +- %.1f (95 %% CI),", n, 100 * p,
          196 * sqrt(p * (1 - p) / n)
        printf " mean spread %+.1f\n", spread / n
      }' | tee -a "$LOG"
  fi
else
  skip_units $((SEEDS * MATCH_GAMES))
fi

if ((smp)); then
  need_csw24
  for threads in "$MAX_THREADS" 1; do
    note "Lazy SMP: $threads endgame thread(s)"
    step $SMP_GAMES "" build/engine/kingbble_selfplay \
      --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --a equity+eg --b equity \
      --threads $threads --endgame-time 1 --games $SMP_GAMES --seed 1 --progress
  done
  ((DRY_RUN)) || grep -E "^\[#34\] Lazy SMP|a win %" "$LOG" | tail -4
fi
