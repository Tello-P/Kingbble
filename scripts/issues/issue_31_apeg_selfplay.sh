#!/usr/bin/env bash
# #31 (and #29): self-play of the approximate pre-endgame, equity+apeg+peg+eg vs
# equity+eg, 2 x 20 games. Check: combined a win % >= 50 and a mean spread >= 0.
#   scripts/issues/issue_31_apeg_selfplay.sh [--dry-run]
source "$(dirname "$0")/lib.sh"

GAMES=20
issue_init 31 $((2 * GAMES)) "$@"
build_engine

for seed in 1 2; do
  step $GAMES "" build/engine/kingbble_selfplay \
    --lexicon lexica/ENABLE.txt --leaves lexica/NWL23.klv2 --a equity+apeg+peg+eg --b equity+eg \
    --games $GAMES --seed $seed --threads 8 \
    --peg-bag 1 --peg-time 5 --endgame-time 2 --apeg-samples 30 --apeg-plies 2 --apeg-time 5 \
    --progress
done
((DRY_RUN)) && exit 0
# Both runs have the same number of games, so the combined figures are plain means.
grep -o "a win %: [-0-9.e]*, a avg spread: [-0-9.e]*" "$LOG" | tail -2 |
  awk -F'[:,] *' '
  { win += $2; spread += $4; n++ }
  END {
    printf "[#31] combined over %d runs: a win %% %.1f, a avg spread %+.1f\n", n, win / n,
      spread / n
  }' |
  tee -a "$LOG"
