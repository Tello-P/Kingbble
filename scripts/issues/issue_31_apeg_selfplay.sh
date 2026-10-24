#!/usr/bin/env bash
# #31 (and #29): self-play of the approximate pre-endgame, equity+apeg+peg+eg vs
# equity+eg, 2 x 20 games. Check: combined a win % >= 50 and a mean spread >= 0.
# --quick: 2 x 10 games with shorter time limits.
# Every finished game/position is kept: stop the script any time and run it again
# to go on (--fresh starts over, --status reports).
#   scripts/issues/issue_31_apeg_selfplay.sh [--dry-run] [--quick] [--fresh] [--status]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
GAMES=20 PEG_TIME=5 EG_TIME=2 APEG_SAMPLES=30 APEG_TIME=5
((QUICK)) && GAMES=10 PEG_TIME=2 EG_TIME=1 APEG_SAMPLES=20 APEG_TIME=2
issue_init 31 $((2 * GAMES)) "$@"
need_csw24
build_engine

for seed in 1 2; do
  step $GAMES "" build/engine/kingbble_selfplay \
    --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --a equity+apeg+peg+eg --b equity+eg \
    --games $GAMES --seed $seed --threads $MAX_THREADS \
    --peg-bag 1 --peg-time $PEG_TIME --endgame-time $EG_TIME --apeg-samples $APEG_SAMPLES \
    --apeg-plies 2 --apeg-time $APEG_TIME \
    --progress --results "$(results_file s$seed)"
done
finish && exit 0
# Both runs have the same number of games, so the combined figures are plain means.
grep -o "a win %: [-0-9.e]*, a avg spread: [-0-9.e]*" "$LOG" | tail -2 |
  awk -F'[:,] *' '
  { win += $2; spread += $4; n++ }
  END {
    printf "[#31] combined over %d runs: a win %% %.1f, a avg spread %+.1f\n", n, win / n,
      spread / n
  }' |
  tee -a "$LOG"
