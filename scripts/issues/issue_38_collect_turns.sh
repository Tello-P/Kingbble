#!/usr/bin/env bash
# #38: download Woogles games and build the turn store until it holds >= 100,000
# human turns with a rack (M3 exit criterion, tag v0.3). Already downloaded games
# are skipped, so a rerun resumes; --max-games counts new games only.
#   scripts/issues/issue_38_collect_turns.sh --players nick1,nick2 [--max-games N] [--lexicon L]
#     [--dry-run]
source "$(dirname "$0")/lib.sh"

PLAYERS=${KINGBBLE_PLAYERS:-} MAX_GAMES=5000 TARGET=100000 LEXICON=CSW24
args=("$@")
for ((i = 0; i < ${#args[@]}; i++)); do
  case ${args[i]} in
    --players) PLAYERS=${args[i + 1]:-} ;;
    --max-games) MAX_GAMES=${args[i + 1]:-} ;;
    --lexicon) LEXICON=${args[i + 1]:-} ;;
  esac
done
if [[ -z $PLAYERS ]]; then
  if [[ " $* " != *" --dry-run "* ]]; then
    echo "usage: $0 --players nick1,nick2 [--max-games N] [--lexicon L] [--dry-run]" \
      "(or set KINGBBLE_PLAYERS)" >&2
    exit 2
  fi
  PLAYERS="<seed list>"
fi
issue_init 38 "$((MAX_GAMES + 1))" "$@"

step "$MAX_GAMES" "" "$PY" -u -m ml.data.woogles --players "$PLAYERS" --max-games "$MAX_GAMES" \
  --delay 1.0 --lexicon "$LEXICON" --progress
step 1 "" "$PY" -m ml.data.store build
step 0 "" "$PY" -m ml.data.store stats --lexicon "$LEXICON"
((DRY_RUN)) && exit 0
have=$(grep -o "^human turns with rack: [0-9]*" "$LOG" | tail -1 | grep -o "[0-9]*$")
if ((${have:-0} >= TARGET)); then
  note "done: $have human turns with rack (>= $TARGET)"
else
  note "not yet: ${have:-0} of $TARGET human turns with rack; rerun to download more"
fi
