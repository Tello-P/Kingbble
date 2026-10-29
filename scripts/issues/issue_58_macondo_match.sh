#!/usr/bin/env bash
# #58: match against Macondo on CSW24 at equal time per move and equal threads.
# Kingbble's sim+peg+eg (2-ply simulation, rack posterior off) plays Macondo's
# simulating bot (simulation, pre-endgame and endgame solvers); each side gets
# --time seconds per move (our simulation, pre-endgame and endgame each stop at
# it; Macondo's whole decision does) and the same threads (Go is capped with
# GOMAXPROCS, both under lib.sh's CPU quota). Games are paired: each deal is
# played twice with the seats swapped. Check: a win % 95 % CI reaching 50.
#
# --quick: 40 games at 5 s per move, about 1 h 30 on 3 threads (+-15 points).
# Full mode: 300 games at 5 s per move, about 10 h (+-6 points).
# Every finished game is kept: stop the script any time and run it again to go
# on (--fresh starts over, --status reports).
# Needs a Macondo checkout in ~/src/macondo (or --macondo DIR) and Go (on the
# PATH or in ~/.local/go/bin); the helper (scripts/macondo_match/main.go) is
# built inside the checkout and its data directory set up in
# build/macondo_match (Macondo's letter distributions and strategy files, our
# CSW24 KWG and CSW24.klv2).
# Resources: --threads N (or KINGBBLE_THREADS, default 3) and --mem SIZE (or
# KINGBBLE_MEM_MAX, default 6G) under lib.sh's LIMITS. It refuses to start while
# another issue script or engine binary runs, unless --force.
#   scripts/issues/issue_58_macondo_match.sh [--quick] [--dry-run] [--fresh] [--status]
#     [--games N] [--time S] [--macondo DIR] [--threads N] [--mem SIZE] [--force]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
GAMES=300 TIME=5 MACONDO=$HOME/src/macondo
((QUICK)) && GAMES=40
args=("$@")
for ((i = 0; i < ${#args[@]}; i++)); do
  case ${args[i]} in
    --games) GAMES=${args[i + 1]} ;;
    --time) TIME=${args[i + 1]} ;;
    --macondo) MACONDO=${args[i + 1]} ;;
  esac
done
GAMES=$((GAMES + GAMES % 2))
GO=$(command -v go || echo "$HOME/.local/go/bin/go")
WORK=$ROOT/build/macondo_match
HELPER=$WORK/kingbble_macondo
# About 24 moves per game at --time seconds each.
GAME_UNITS=$(awk -v t="$TIME" 'BEGIN { printf "%d", 24 * t + 0.5 }')

limits_init "$@"
issue_init 58 $((GAMES * GAME_UNITS)) "$@"
((STATUS || DRY_RUN)) || busy_check 58
need_csw24
need "$MACONDO" "clone https://github.com/domino14/macondo to ~/src/macondo"
build_engine

# Macondo's data directory and the helper, rebuilt when missing.
if ((!STATUS)); then
  if ((DRY_RUN)); then
    echo "set up $WORK/data and build $HELPER in $MACONDO"
  else
    mkdir -p "$WORK/data/lexica/gaddag"
    cp -r "$MACONDO/data/letterdistributions" "$MACONDO/data/strategy" "$WORK/data/"
    [[ -e $WORK/data/lexica/gaddag/CSW24.kwg ]] ||
      "${LIMITS[@]}" build/engine/kingbble_buildkwg lexica/CSW24.txt \
        "$WORK/data/lexica/gaddag/CSW24.kwg" >>"$LOG" 2>&1
    cp lexica/CSW24.klv2 "$WORK/data/lexica/gaddag/CSW24.klv2"
    mkdir -p "$MACONDO/cmd/kingbble_macondo"
    cp scripts/macondo_match/main.go "$MACONDO/cmd/kingbble_macondo/main.go"
    (cd "$MACONDO" && "${LIMITS[@]}" "$GO" build -o "$HELPER" ./cmd/kingbble_macondo) >>"$LOG" 2>&1
  fi
fi

STEP_SCALE=$GAME_UNITS step "$GAMES" "" build/engine/kingbble_macondo_match \
  --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --helper "$HELPER" \
  --macondo-data "$WORK/data" --games "$GAMES" --seed 1 --time "$TIME" \
  --threads "$MAX_THREADS" --progress --results "$(results_file "t$TIME")"
