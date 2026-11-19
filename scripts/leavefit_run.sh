#!/usr/bin/env bash
# Fits leave corrections from a merged leavegen run (half-a.bin, half-b.bin) on
# the runner. Writes only corrections ("leave,delta" CSVs) to OUT, for targets
# a (equity) and b (win chance), never a leave table.
#   scripts/leavefit_run.sh MERGE_RUN [PRIOR_DELTAS_RUN]
set -euo pipefail
merge=$1 prior=${2:-}
repo=$GITHUB_REPOSITORY
until [[ $(gh run view "$merge" -R "$repo" --json status -q .status) == completed ]]; do
  sleep 60
done
gh run download "$merge" -R "$repo" -D halves -n shard-0
[[ -s halves/half-a.bin && -s halves/half-b.bin ]] || { echo "::error::no halves in $merge"; exit 1; }
extra=()
if [[ -n $prior ]]; then
  gh run download "$prior" -R "$repo" -D prior -n shard-0
  extra=(--leave-deltas prior/deltas.csv)
fi
for t in a b; do
  build/engine/kingbble_leavefit --leaves lexica/CSW24.klv2 "${extra[@]}" --half-a halves/half-a.bin \
    --half-b halves/half-b.bin --target "$t" --out "$OUT/deltas_$t.csv" | tee "$OUT/fit_$t.txt"
  [[ $(wc -l < "$OUT/deltas_$t.csv") -gt 1 ]] || echo "::warning::target $t wrote no corrections"
done
