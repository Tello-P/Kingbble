#!/usr/bin/env bash
# Kingbble (WMP + pre-endgame) against Quackle Championship at equal time per move
# (35 s, as Quackle takes), run inside an Actions job: waits for a rehearsal run
# (one shard, two games), checks it finished both, then starts three runs of 20
# shards x 35 games (2,100 games) on seeds 31000000, 32000000 and 33000000.
#   scripts/quackle_chain.sh REHEARSAL_RUN
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
for seed in 31000000 32000000 33000000; do
  gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=20 -f seed="$seed" \
    -f minutos=360 -f minimo=15 -f csw24=true -f quackle=true \
    -f comando="mkdir -p \$OUT/gcg; build/engine/kingbble_quackle_match --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --helper build/quackle_helper/quackle_helper --threads \$KINGBBLE_MAX_THREADS --quackle-threads \$KINGBBLE_MAX_THREADS --seed \$SEED --gcg-dir \$OUT/gcg --results \$OUT/match.txt --bot sim+peg+eg --games 35 --sim-time 35 --sim-iterations 100000 --late-candidates 30 --keep-bag-candidates 5 --wmp"
done
echo "match started"
