#!/usr/bin/env bash
# Starts round 2 of the Phase 4 loop once round 1 is over: waits for round 1's chain
# (AFTER_RUN) and then for every other run of the workflow to finish, so it takes
# no slot from them, and dispatches the data run and its chain. A job lasts at most
# 6 h: near that it starts a copy of itself and stops.
#   scripts/value_round2.sh AFTER_RUN
set -euo pipefail
after=$1 repo=$GITHUB_REPOSITORY start=$SECONDS
busy() {
  gh run list -R "$repo" --workflow pruebas.yml --limit 50 --json databaseId,status \
    -q "[.[] | select(.databaseId != $GITHUB_RUN_ID and .status != \"completed\")] | length"
}
while [[ $(gh run view "$after" -R "$repo" --json status -q .status) != completed ]] || (($(busy) > 0)); do
  if ((SECONDS - start > 5 * 3600)); then
    gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=1 -f seed=1 \
      -f minutos=330 -f comando="CHECK_RUN=${CHECK_RUN:-} scripts/value_round2.sh $after"
    echo "handed over to a new run"
    exit 0
  fi
  sleep 120
done
# A rehearsal chain (CHECK_RUN) must have passed, or round 2 is not started.
if [[ -n ${CHECK_RUN:-} ]]; then
  # A timed-out job also ends green, so the rehearsal must have left a model.
  until [[ $(gh run view "$CHECK_RUN" -R "$repo" --json status -q .status) == completed ]]; do
    sleep 60
  done
  gh run download "$CHECK_RUN" -R "$repo" -D rehearsal -n shard-0
  [[ -s rehearsal/value_model.txt ]] || { echo "::error::rehearsal $CHECK_RUN left no model"; exit 1; }
fi
gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=19 -f seed=171000000 \
  -f minutos=280 -f csw24=true -f comando='for i in 0 1; do mkdir -p $OUT/p$i; build/engine/kingbble_valuedata --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --games 100000 --seed $((SEED + i * 50000)) --sim-time 0.3 --sample 0.15 --rollouts 16 --out $OUT/p$i/value.csv & done; wait'
sleep 20
data=$(gh run list -R "$repo" --workflow pruebas.yml --limit 1 --json databaseId -q '.[0].databaseId')
gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=1 -f seed=2 -f minutos=360 \
  -f csw24=true -f comando="LABELS='game rollout mix' JUDGE=game MACONDO_GAMES=100 scripts/value_chain.sh $data 181000000 23000000"
echo "round 2: data run $data and its chain started"
