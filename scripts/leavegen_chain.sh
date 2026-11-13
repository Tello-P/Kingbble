#!/usr/bin/env bash
# Leave-generation data on GitHub (generation 0, from the published CSW24 leaves):
# waits for a rehearsal run (one shard, a few thousand games), checks it wrote
# samples, then starts 20 shards that each run two processes of GAMES games.
# Only the per-rack sums leave the runners, never a leave table.
#   scripts/leavegen_chain.sh REHEARSAL_RUN GAMES_PER_PROCESS
set -euo pipefail
rehearsal=$1 games=$2
repo=$GITHUB_REPOSITORY
until [[ $(gh run view "$rehearsal" -R "$repo" --json status -q .status) == completed ]]; do
  sleep 60
done
gh run download "$rehearsal" -R "$repo" -D rehearsal -n shard-0
grep -q "^games .* samples [1-9]" rehearsal/log.txt || { echo "::error::rehearsal $rehearsal wrote no samples"; exit 1; }
[[ -s rehearsal/lg-0.bin ]] || { echo "::error::rehearsal $rehearsal left no data"; exit 1; }
gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=20 -f seed=101000000 \
  -f minutos=360 -f csw24=true \
  -f comando="for p in 0 1; do build/engine/kingbble_leavegen --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --win-table configs/win_table_csw24.csv --games $games --seed \$((SEED + p * 1000)) --wmp --out \$OUT/lg-\$p.bin & done; wait; test -s \$OUT/lg-0.bin && test -s \$OUT/lg-1.bin"
echo "generation 0 started"
