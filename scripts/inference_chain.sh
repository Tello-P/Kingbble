#!/usr/bin/env bash
# Rack inference against Macondo, run inside an Actions job: waits for a
# rehearsal run (one shard, a couple of games with the same bot), checks it
# finished games and tracked Macondo's rack, then starts the full match on A2's
# seeds and settings. Fails loudly at any step.
#   scripts/inference_chain.sh REHEARSAL_RUN MODEL [GAMES_PER_SHARD]
# MODEL is the "+inf:" move model (softrf, rf, human); extra match flags go in
# MATCH_FLAGS (e.g. "--tau 4").
set -euo pipefail
rehearsal=$1 model=$2 games=${3:-100}
repo=$GITHUB_REPOSITORY

until [[ $(gh run view "$rehearsal" -R "$repo" --json status -q .status) == completed ]]; do
  sleep 60
done
gh run download "$rehearsal" -R "$repo" -D rehearsal -n shard-0
played=$(grep -c '^game [0-9]*:' rehearsal/match.txt || true)
tracked=$(grep -c '^inference game' rehearsal/log.txt || true)
echo "rehearsal: $played games, $tracked with inference"
((played >= 2 && tracked >= 2)) || { echo "::error::rehearsal $rehearsal did not play tracked games"; exit 1; }
grep '^inference game' rehearsal/log.txt

# A2 (--end-unseen 14 --late-bag 7, Kingbble's final settings) on A2's seeds.
gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=20 -f seed=23000000 \
  -f minutos=360 -f minimo=20 -f csw24=true -f macondo=true -f macondo_ref=v0.13.9 \
  -f comando="build/engine/kingbble_macondo_match --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --helper build/macondo_match/kingbble_macondo --macondo-data build/macondo_match/data --bot sim+inf:$model+peg+eg --games $games --seed \$SEED --time 5 --threads \$KINGBBLE_MAX_THREADS --end-unseen 14 --late-bag 7 ${MATCH_FLAGS:-} --stats \$OUT/stats.txt --gcg-dir \$OUT/gcg --results \$OUT/match.txt"
echo "match started"
