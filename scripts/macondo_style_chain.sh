#!/usr/bin/env bash
# Macondo-style simulation against Macondo, run inside an Actions job: waits for
# a rehearsal run (one shard, a couple of games), checks it finished them, then
# starts 20 shards of 80 games on each of the two seeds where A2 has 100 games a
# shard (23000000, and 24000000 from the repeat), with A2's settings. Fails loudly.
#   scripts/macondo_style_chain.sh REHEARSAL_RUN [GAMES_PER_SHARD]
set -euo pipefail
rehearsal=$1 games=${2:-80}
repo=$GITHUB_REPOSITORY

until [[ $(gh run view "$rehearsal" -R "$repo" --json status -q .status) == completed ]]; do
  sleep 60
done
gh run download "$rehearsal" -R "$repo" -D rehearsal -n shard-0
played=$(grep -c '^game [0-9]*:' rehearsal/match.txt || true)
echo "rehearsal: $played games"
((played >= 2)) || { echo "::error::rehearsal $rehearsal did not finish its games"; exit 1; }

# A2 (--end-unseen 14 --late-bag 7, Kingbble's final settings) on A2's seeds.
for seed in 23000000 24000000; do
  gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=20 -f seed="$seed" \
    -f minutos=360 -f minimo=40 -f csw24=true -f macondo=true -f macondo_ref=v0.13.9 \
    -f comando="build/engine/kingbble_macondo_match --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --helper build/macondo_match/kingbble_macondo --macondo-data build/macondo_match/data --bot sim+peg+eg --macondo-style --win-pct build/macondo_match/data/strategy/default/winpct.csv --games $games --seed \$SEED --time 5 --threads \$KINGBBLE_MAX_THREADS --end-unseen 14 --late-bag 7 --stats \$OUT/stats.txt --gcg-dir \$OUT/gcg --results \$OUT/match.txt"
done
echo "match started"
