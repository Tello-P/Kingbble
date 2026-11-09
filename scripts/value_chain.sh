#!/usr/bin/env bash
# Phase 4 chain, run inside an Actions job: waits for a value data run, trains the
# value model and, only if it beats spread and unseen tiles on held-out rows,
# starts the self-play and Macondo runs that test it (they wait for this run and
# take its model). Fails loudly at any step.
#   scripts/value_chain.sh DATA_RUNS SELFPLAY_SEED MACONDO_SEED [GAME_RUN...]
# DATA_RUNS: one run id or several joined by commas; the chain waits for the first.
# With GAME_RUN ids, rows from those runs' games against Macondo are added.
# MIN_ROWS, TEST_SHARDS, SELFPLAY_GAMES and MACONDO_GAMES shrink it for a rehearsal.
set -euo pipefail
data_run=$1 selfplay_seed=$2 macondo_seed=$3
shift 3
repo=$GITHUB_REPOSITORY out=${OUT:?}
min_rows=${MIN_ROWS:-20000} shards=${TEST_SHARDS:-20}
selfplay_games=${SELFPLAY_GAMES:-100} macondo_games=${MACONDO_GAMES:-50}

until [[ $(gh run view "${data_run%%,*}" -R "$repo" --json status -q .status) == completed ]]; do
  sleep 60
done
pip install -q lightgbm
for run in ${data_run//,/ }; do
  gh run download "$run" -R "$repo" -D "input/sim/$run" --pattern "shard-*"
done
for run in "$@"; do
  gh run download "$run" -R "$repo" -D "games/$run" --pattern "shard-*"
  mkdir -p "input/macondo/$run"
  build/engine/kingbble_valuedata --leaves lexica/CSW24.klv2 --from-gcg "games/$run" \
    --out "input/macondo/$run/value.csv"
done
rows=$(cat $(find input -name value.csv) | wc -l)
echo "rows: $rows"
((rows >= min_rows)) || { echo "::error::only $rows rows"; exit 1; }

# Every label in LABELS is trained and scored on the held-out rows' JUDGE label; the
# one with the lowest log loss becomes value_model.txt.
best="" best_loss=""
for label in ${LABELS:-game}; do
  python scripts/train_value.py input --label "$label" --judge "${JUDGE:-rollout}" \
    --out "$out/value_model_$label.txt" --engine build/engine/kingbble_valuedata |
    tee "$out/train_$label.txt"
  [[ -s $out/value_model_$label.txt ]] || { echo "::error::no model for $label"; exit 1; }
  loss=$(awk '/^value model:/ {print $5}' "$out/train_$label.txt" | tr -d ,)
  table=$(awk '/^spread\+unseen:/ {print $4}' "$out/train_$label.txt" | tr -d ,)
  awk -v m="$loss" -v t="$table" 'BEGIN { exit !(m != "" && t != "" && m < t) }' || {
    if [[ -n ${SKIP_GUARD:-} ]]; then  # a rehearsal has too few rows to beat it
      echo "::warning::$label: log loss $loss is not below spread+unseen $table"
    else
      echo "::error::$label: log loss $loss is not below spread+unseen $table"; exit 1
    fi
  }
  if [[ -z $best_loss ]] || awk -v a="$loss" -v b="$best_loss" 'BEGIN { exit !(a < b) }'; then
    best=$label best_loss=$loss
  fi
done
cp "$out/value_model_$best.txt" "$out/value_model.txt"
echo "chosen label: $best (held-out log loss $best_loss)"

me=$GITHUB_RUN_ID
wait_model="until [ \"\$(gh run view $me -R $repo --json status -q .status)\" = completed ]; do sleep 30; done; gh run download $me -R $repo -D model -n shard-0"
gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards="$shards" -f seed="$selfplay_seed" \
  -f minutos=240 -f csw24=true -f comando="$wait_model && build/engine/kingbble_selfplay --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --a sim+peg+eg --b sim+peg+eg --a-value-model model/value_model.txt --paired --games $selfplay_games --seed \$SEED --sim-time 1 --sim-iterations 100000 --threads \$KINGBBLE_MAX_THREADS --results \$OUT/match.txt"
gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards="$shards" -f seed="$macondo_seed" \
  -f minutos=300 -f csw24=true -f macondo=true -f macondo_ref=v0.13.9 -f comando="$wait_model && build/engine/kingbble_macondo_match --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --helper build/macondo_match/kingbble_macondo --macondo-data build/macondo_match/data --bot sim+peg+eg --games $macondo_games --seed \$SEED --time 5 --threads \$KINGBBLE_MAX_THREADS --value-model model/value_model.txt --stats \$OUT/stats.txt --gcg-dir \$OUT/gcg --results \$OUT/match.txt"
echo "tests started"
