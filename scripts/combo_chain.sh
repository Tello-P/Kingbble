#!/usr/bin/env bash
# WMP + pre-endgame against Macondo, run inside an Actions job: waits for a
# rehearsal run (one shard, two games), checks it finished both, then starts 20
# shards of 80 games on each of A2's seed sets (23000000 and 24000000), with A2's
# settings. Fails loudly.
#   scripts/combo_chain.sh REHEARSAL_RUN
set -euo pipefail
rehearsal=$1
repo=$GITHUB_REPOSITORY
until [[ $(gh run view "$rehearsal" -R "$repo" --json status -q .status) == completed ]]; do
  sleep 60
done
gh run download "$rehearsal" -R "$repo" -D rehearsal -n shard-0
played=$(grep -c '^game [0-9]*:' rehearsal/match.txt || true)
echo "rehearsal: $played games"
((played >= 2)) || { echo "::error::rehearsal $rehearsal did not finish its games"; exit 1; }
for seed in 23000000 24000000; do
  gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=20 -f seed="$seed" \
    -f minutos=360 -f minimo=40 -f csw24=true -f macondo=true -f macondo_ref=v0.13.9 \
    -f comando="build/engine/kingbble_macondo_match --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --helper build/macondo_match/kingbble_macondo --macondo-data build/macondo_match/data --bot sim+peg+eg --games 80 --seed \$SEED --time 5 --threads \$KINGBBLE_MAX_THREADS --end-unseen 14 --late-bag 7 --late-candidates 30 --keep-bag-candidates 5 --wmp --stats \$OUT/stats.txt --gcg-dir \$OUT/gcg --results \$OUT/match.txt"
done
echo "match started"
