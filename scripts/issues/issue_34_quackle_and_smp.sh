#!/usr/bin/env bash
# #34 (and #21, #24):
#  1. Quackle match, sim+peg+eg against Championship Player, 2,000 games (--games N
#     for fewer). Check: Kingbble win % >= 45 (v0.2 exit criterion). About 4 min a
#     game on 3 cores.
#  2. Lazy SMP endgame: equity+eg vs equity, 200 games with MAX_THREADS and with
#     1 endgame thread. Check: more threads no worse in a win % and a avg spread.
# Every finished game is kept: stop the script any time and run it again to go on
# (--fresh starts over, --status reports).
# --quick: 10 match games against Quackle's Speedy Player with a lighter Kingbble
# (40 simulations, 2 s pre-endgame and endgame), and 2 x 20 Lazy SMP games.
#   scripts/issues/issue_34_quackle_and_smp.sh [--dry-run] [--quick] [--fresh] [--status]
#       [--games N] [--match-only | --smp-only]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
MATCH_GAMES=2000 SMP_GAMES=200
MATCH_ARGS=()
if ((QUICK)); then
  MATCH_GAMES=10 SMP_GAMES=20
  MATCH_ARGS=(--quackle-player "Speedy Player" --sim-iterations 40 --peg-time 2 --endgame-time 2)
fi
args=("$@")
for ((i = 0; i < ${#args[@]}; ++i)); do
  [[ ${args[i]} == --games ]] && MATCH_GAMES=${args[i + 1]:?--games needs a value}
done
issue_init 34 $((MATCH_GAMES + 2 * SMP_GAMES)) "$@"
match=1 smp=1
for ((i = 0; i < ${#ARGS[@]}; ++i)); do
  case ${ARGS[i]} in
    --match-only) smp=0 ;;
    --smp-only) match=0 ;;
    --games) i=$((i + 1)) ;;
    *) echo "unknown argument: ${ARGS[i]}" >&2; exit 2 ;;
  esac
done
build_engine

if ((match)); then
  QUACKLE=$HOME/src/quackle
  if ((!STATUS)) && { ((DRY_RUN)) || [[ ! -x build/quackle_helper/quackle_helper ]]; }; then
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
  note "Quackle match, $MATCH_GAMES games"
  step $MATCH_GAMES "" build/engine/kingbble_quackle_match \
    --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 \
    --helper build/quackle_helper/quackle_helper --bot sim+peg+eg --threads $MAX_THREADS \
    --quackle-threads $MAX_THREADS --games $MATCH_GAMES --seed 1 \
    --gcg-dir "build/quackle_games/match$(quick_suffix)" \
    --results "$(results_file match)" "${MATCH_ARGS[@]}"
else
  skip_units $MATCH_GAMES
fi

if ((smp)); then
  need_csw24
  for threads in "$MAX_THREADS" 1; do
    note "Lazy SMP: $threads endgame thread(s)"
    step $SMP_GAMES "" build/engine/kingbble_selfplay \
      --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --a equity+eg --b equity \
      --threads $threads --endgame-time 1 --games $SMP_GAMES --seed 1 --progress \
      --results "$(results_file smp_t$threads)"
  done
  finish || grep -E "^\[#34\] Lazy SMP|a win %" "$LOG" | tail -4
fi
