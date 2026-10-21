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
#   scripts/issues/issue_47_inference_eval.sh [--dry-run] [--games N] [--max-turns N]
source "$(dirname "$0")/lib.sh"

GAMES=100 MAX_TURNS=2000 RARE=0.3
args=("$@")
for ((i = 0; i < ${#args[@]}; i++)); do
  case ${args[i]} in
    --games) GAMES=${args[i + 1]:-} ;;
    --max-turns) MAX_TURNS=${args[i + 1]:-} ;;
  esac
done
RAW=data/raw/woogles
FILES=$( (find "$RAW" -maxdepth 1 -name '*.gcg' 2>/dev/null || true) | wc -l)
((FILES > 0)) || FILES=1
# Units: features and human eval walk every game file; each self-play game is one more.
issue_init 47 $((2 * FILES + 1 + 4 * GAMES)) "$@"
need_csw24
need "$RAW" "download the games first: scripts/issues/issue_38_collect_turns.sh"
need data/turns "build the turn store first: scripts/issues/issue_38_collect_turns.sh"
build_engine

OUT=build/issue_runs/issue_47
LEX=(--lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2)
MODEL=models/human_model.txt
EVAL=build/engine/kingbble_inference_eval
SIM=(--sim-candidates 10 --sim-iterations 100 --threads 8)
((DRY_RUN)) || mkdir -p "$OUT"

step "$FILES" "" "$PY" -m ml.data.features "${LEX[@]}" --raw "$RAW" --progress
step 1 "" "$PY" -m ml.human_model.logit --split train --out "$MODEL"
if ((DRY_RUN)); then
  echo "$PY -c '<test split turns of data/features>' > $OUT/test_turns.tsv"
else
  "$PY" -c '
import sys
from ml.data.features import ratings
for (game, turn), (rating, split) in sorted(ratings().items()):
    if split == "test":
        print(f"{game}\t{turn}\t{rating or 0}")
' >"$OUT/test_turns.tsv"
  note "test turns: $(wc -l <"$OUT/test_turns.tsv")"
fi
step "$FILES" "" "$EVAL" "${LEX[@]}" --human-params "$MODEL" --games "$RAW" \
  --turns "$OUT/test_turns.tsv" --models none,rangefinder,human --max-turns "$MAX_TURNS" \
  --samples 500 --records "$OUT/human_games.jsonl" --progress
step 0 "" "$PY" -m ml.eval.metrics "$OUT/human_games.jsonl"

for seed in 1 2; do
  step "$GAMES" "" "$EVAL" "${LEX[@]}" --human-params "$MODEL" --a human --b rangefinder \
    --games-played "$GAMES" --seed "$seed" "${SIM[@]}" --records "$OUT/selfplay_$seed.jsonl" \
    --progress
done
for a in human rangefinder; do
  step "$GAMES" "" "$EVAL" "${LEX[@]}" --human-params "$MODEL" --a "$a" --b none --b-rare "$RARE" \
    --games-played "$GAMES" --seed 3 "${SIM[@]}" --records "$OUT/stress_$a.jsonl" --progress
done
step 0 "" "$PY" -m ml.eval.metrics "$OUT"/selfplay_*.jsonl "$OUT"/stress_*.jsonl
((DRY_RUN)) && exit 0

loss() { grep -o "^$1: n=[0-9]* log_loss=[-0-9.na]*" "$LOG" | tail -1 | grep -o "[-0-9.na]*$"; }
win() { grep -o "^a=$1 vs b=$2 (rare a/b: 0/$3).*a win %: [-0-9.e]*" "$LOG" | tail -${4:-1} |
  grep -o "[-0-9.e]*$" | awk '{ s += $1; n++ } END { if (n) printf "%.1f", s / n }'; }
note "log-loss on test turns: human $(loss human), rangefinder $(loss rangefinder), none $(loss none)"
note "self-play human vs rangefinder, combined human win %: $(win human rangefinder 0 2)"
note "stress (rival rare $RARE), win %: human $(win human none "$RARE"), rangefinder $(win rangefinder none "$RARE")"
