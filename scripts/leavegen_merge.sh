#!/usr/bin/env bash
# Merges a leavegen run's shards into two independent halves (even and odd
# shards) for kingbble_leavefit's split-half estimate. Sums only.
#   scripts/leavegen_merge.sh DATA_RUN
set -euo pipefail
run=$1
repo=$GITHUB_REPOSITORY
until [[ $(gh run view "$run" -R "$repo" --json status -q .status) == completed ]]; do
  sleep 60
done
gh run download "$run" -R "$repo" -D data --pattern "shard-*"
even=() odd=()
for d in data/shard-*; do
  n=${d##*-}
  for f in "$d"/lg-*.bin; do
    [[ -s $f ]] || { echo "::error::$f is empty"; exit 1; }
    if ((n % 2 == 0)); then even+=("$f"); else odd+=("$f"); fi
  done
done
((${#even[@]} >= 10 && ${#odd[@]} >= 10)) || { echo "::error::too few shard files: ${#even[@]} even, ${#odd[@]} odd"; exit 1; }
build/engine/kingbble_leavegen --merge "${even[@]}" --out "$OUT/half-a.bin"
build/engine/kingbble_leavegen --merge "${odd[@]}" --out "$OUT/half-b.bin"
ls -la "$OUT"
