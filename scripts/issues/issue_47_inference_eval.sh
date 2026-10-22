#!/usr/bin/env bash
# #47: evaluation of the rack inference on CSW24 (M4 exit criterion, tag v0.4).
#   1. per-option features of the Woogles turns (#38) and the human move model
#      fitted on the train split;
#   2. oracle metrics on held-out (test split) human turns: none, rangefinder, human;
#   3. self-play, human inference vs the RangeFinder baseline, 2 x GAMES games;
#   4. stress test: a rival playing a random option ranked 10th or lower 30 % of
#      the time, against human inference and against the baseline, GAMES games each.
# Check: human beats rangefinder in log-loss and in combined win % (>= 50), and
# its win % against the rare-move rival is not below the baseline's.
#
# Full mode: 100 games per run, 10 candidates x 100 iterations (about 2.5 h on 8
# threads). --quick: 30 games per run, 8 candidates x 50 iterations (about 1 h).
# Both share steps 1-2. Every step resumes: finished steps leave a .done marker,
# the evaluation app skips the game files and games listed in <records>.done,
# so an interrupted run continues where it stopped.
#   scripts/issues/issue_47_inference_eval.sh [--quick] [--dry-run] [--games N] [--max-turns N]
source "$(dirname "$0")/lib.sh"

MODE=full GAMES=100 MAX_TURNS=2000 RARE=0.3
SIM=(--sim-candidates 10 --sim-iterations 100 --threads 8)
GAME_UNITS=160  # measured seconds per game x 10 (the script's unit is 0.1 s)
args=("$@")
for a in "${args[@]}"; do
  if [[ $a == --quick ]]; then
    MODE=quick GAMES=30 GAME_UNITS=115
    SIM=(--sim-candidates 8 --sim-iterations 50 --threads 8)
  fi
done
for ((i = 0; i < ${#args[@]}; i++)); do
  case ${args[i]} in
    --games) GAMES=${args[i + 1]:-} ;;
    --max-turns) MAX_TURNS=${args[i + 1]:-} ;;
  esac
done

RAW=data/raw/woogles
OUT=build/issue_runs/issue_47
RUNS=$OUT/$MODE
LEX=(--lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2)
MODEL=models/human_model.txt
EVAL=build/engine/kingbble_inference_eval
FILES=$( (find "$RAW" -maxdepth 1 -name '*.gcg' 2>/dev/null || true) | wc -l)
((FILES > 0)) || FILES=1
FIT_UNITS=900    # about 90 s
TURN_UNITS=10    # about 1 s per test turn (three models)
# Self-play runs: name, then the app's arguments.
PLAY=(
  "selfplay_1 --a human --b rangefinder --seed 1"
  "selfplay_2 --a human --b rangefinder --seed 2"
  "stress_human --a human --b none --b-rare $RARE --seed 3"
  "stress_rangefinder --a rangefinder --b none --b-rare $RARE --seed 3"
)

is_done() { [[ -e $OUT/$1.done ]]; }
mark_done() { ((DRY_RUN)) || touch "$OUT/$1.done"; }
# Lines in a records journal: the units it has finished.
finished() { if [[ -e $1.done ]]; then wc -l <"$1.done"; else echo 0; fi; }
scored_turns() {
  if [[ -e $OUT/human_games.jsonl.done ]]; then
    awk '{ s += $3 } END { print s + 0 }' "$OUT/human_games.jsonl.done"
  else
    echo 0
  fi
}

# Total and already finished work, in 0.1 s units.
TOTAL_UNITS=$((FILES + FIT_UNITS + MAX_TURNS * TURN_UNITS + ${#PLAY[@]} * GAMES * GAME_UNITS))
DONE_UNITS=0
is_done features && DONE_UNITS=$((DONE_UNITS + FILES))
is_done fit && DONE_UNITS=$((DONE_UNITS + FIT_UNITS))
SCORED=$(scored_turns)
((SCORED > MAX_TURNS)) && SCORED=$MAX_TURNS
DONE_UNITS=$((DONE_UNITS + SCORED * TURN_UNITS))
for run in "${PLAY[@]}"; do
  read -r name _ <<<"$run"
  n=$(finished "$RUNS/$name.jsonl")
  ((n > GAMES)) && n=$GAMES
  DONE_UNITS=$((DONE_UNITS + n * GAME_UNITS))
done

issue_init 47 "$TOTAL_UNITS" "$@"
need_csw24
need "$RAW" "download the games first: scripts/issues/issue_38_collect_turns.sh"
need data/turns "build the turn store first: scripts/issues/issue_38_collect_turns.sh"
if ((!DRY_RUN)) && ! "$PY" -c 'import numpy, duckdb' 2>/dev/null; then
  echo "[#47] $PY lacks numpy or duckdb: $PY -m pip install -e '.[data,ml]'" >&2
  exit 1
fi
build_engine
((DRY_RUN)) || mkdir -p "$RUNS"
skip_units "$DONE_UNITS"
((DONE_UNITS)) && note "resuming in $MODE mode: $((100 * DONE_UNITS / TOTAL_UNITS))% already done"

if ! is_done features; then
  step "$FILES" "" "$PY" -m ml.data.features "${LEX[@]}" --raw "$RAW" --progress
  mark_done features
fi
if ! is_done fit; then
  STEP_SCALE=$FIT_UNITS step 1 "" "$PY" -m ml.human_model.logit --split train --out "$MODEL"
  mark_done fit
fi
if [[ ! -e $OUT/test_turns.tsv ]]; then
  if ((DRY_RUN)); then
    echo "$PY -c '<test split turns of data/features>' > $OUT/test_turns.tsv"
  else
    "$PY" -c '
from ml.data.features import ratings
for (game, turn), (rating, split) in sorted(ratings().items()):
    if split == "test":
        print(f"{game}\t{turn}\t{rating or 0}")
' >"$OUT/test_turns.tsv.tmp"
    mv "$OUT/test_turns.tsv.tmp" "$OUT/test_turns.tsv"
    note "test turns: $(wc -l <"$OUT/test_turns.tsv")"
  fi
fi
if ! is_done human_games; then
  STEP_SCALE=$TURN_UNITS step $((MAX_TURNS - SCORED)) "" "$EVAL" "${LEX[@]}" \
    --human-params "$MODEL" --games "$RAW" --turns "$OUT/test_turns.tsv" \
    --models none,rangefinder,human --max-turns "$MAX_TURNS" --samples 500 \
    --records "$OUT/human_games.jsonl" --progress
  mark_done human_games
fi
step 0 "" "$PY" -m ml.eval.metrics "$OUT/human_games.jsonl"

for run in "${PLAY[@]}"; do
  read -r name rest <<<"$run"
  read -ra run_args <<<"$rest"
  left=$((GAMES - $(finished "$RUNS/$name.jsonl")))
  ((left > 0)) || continue
  STEP_SCALE=$GAME_UNITS step "$left" "" "$EVAL" "${LEX[@]}" --human-params "$MODEL" \
    "${run_args[@]}" --games-played "$GAMES" "${SIM[@]}" --records "$RUNS/$name.jsonl" --progress
done
step 0 "" "$PY" -m ml.eval.metrics "$RUNS"/selfplay_*.jsonl "$RUNS"/stress_*.jsonl
((DRY_RUN)) && exit 0

loss() { grep -o "^$1: n=[0-9]* log_loss=[-0-9.na]*" "$LOG" | tail -1 | grep -o "[-0-9.na]*$"; }
# Win % of side a over the finished games of the given runs (journal columns:
# key, offset, a's score, b's score, ...).
win() {
  local files=()
  for name in "$@"; do files+=("$RUNS/$name.jsonl.done"); done
  awk '{ n++; w += ($3 > $4) + 0.5 * ($3 == $4) } END { if (n) printf "%.1f (%d games)", 100 * w / n, n }' \
    "${files[@]}"
}
note "$MODE: log-loss on test turns: human $(loss human), rangefinder $(loss rangefinder), none $(loss none)"
note "$MODE: self-play human vs rangefinder, human win %: $(win selfplay_1 selfplay_2)"
note "$MODE: stress (rival rare $RARE), win %: human $(win stress_human), rangefinder $(win stress_rangefinder)"
