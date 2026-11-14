#!/usr/bin/env bash
# Leaves built from scratch (no published values anywhere): one generation per
# run of this script, which chains itself. The table is all-zero leaves plus
# the cumulative corrections (cum.csv) of the generations so far.
#   scripts/scratch_gen.sh LEXICON GEN LAST_GEN PREVIOUS_RUN|none GAMES_PER_PROCESS
# LEXICON: CSW24 or ENABLE (lexica/LEXICON.txt). This run: dispatches a data run
# (20 shards x 4 processes) playing with the current table, merges its sums into
# two halves, fits target a (equity) anchored only to the current table, writes
# OUT/cum.csv and, before the last generation, starts the next one.
set -euo pipefail
lex=$1 gen=$2 last=$3 prev=$4 games=$5
repo=$GITHUB_REPOSITORY
csw=false
[[ $lex == CSW24 ]] && csw=true
python3 scripts/zero_leaves.py > zero.csv
echo "leave,delta" > cum.csv
fetch_prev=""
if [[ $prev != none ]]; then
  gh run download "$prev" -R "$repo" -D prev -n shard-0
  cp prev/cum.csv cum.csv
  fetch_prev="gh run download $prev -R \$GITHUB_REPOSITORY -D prev -n shard-0 && cp prev/cum.csv cum.csv && "
fi
# DATA_RUN (optional): an earlier data run of this generation to use instead of a new one.
if [[ -n ${DATA_RUN:-} ]]; then
  data=$DATA_RUN
else
  since=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=20 -f seed=$((300000000 + gen * 10000000)) \
    -f minutos=300 -f csw24=$csw \
    -f comando="python3 scripts/zero_leaves.py > zero.csv && echo leave,delta > cum.csv && ${fetch_prev}for p in 0 1 2 3; do build/engine/kingbble_leavegen --lexicon lexica/$lex.txt --leaves zero.csv --leave-deltas cum.csv --win-table configs/win_table_csw24.csv --games $games --seed \$((SEED + p * 1000)) --wmp --out \$OUT/lg-\$p.bin & done; wait; for p in 0 1 2 3; do test -s \$OUT/lg-\$p.bin; done"
  # The new run can take a while to be listed: look for it for up to 10 minutes.
  data=""
  for try in $(seq 1 20); do
    sleep 30
    data=$(gh run list -R "$repo" --workflow pruebas.yml --branch "$GITHUB_REF_NAME" --limit 10 \
      --json databaseId,createdAt,event -q "[.[] | select(.createdAt >= \"$since\" and .databaseId != $GITHUB_RUN_ID)] | sort_by(.createdAt) | .[0].databaseId // empty")
    [[ -n $data ]] && break
  done
  [[ -n $data ]] || { echo "::error::could not find the data run"; exit 1; }
fi
echo "generation $gen data run: $data"
until [[ $(gh run view "$data" -R "$repo" --json status -q .status) == completed ]]; do sleep 60; done
[[ $(gh run view "$data" -R "$repo" --json conclusion -q .conclusion) == success ]] || { echo "::error::data run $data failed"; exit 1; }
gh run download "$data" -R "$repo" -D data --pattern "shard-*"
even=() odd=()
for d in data/shard-*; do
  n=${d##*-}
  for f in "$d"/lg-*.bin; do if ((n % 2 == 0)); then even+=("$f"); else odd+=("$f"); fi; done
done
build/engine/kingbble_leavegen --merge "${even[@]}" --out half-a.bin
build/engine/kingbble_leavegen --merge "${odd[@]}" --out half-b.bin
build/engine/kingbble_leavefit --leaves zero.csv --leave-deltas cum.csv --half-a half-a.bin --half-b half-b.bin \
  --target a --cap 100 --min-delta 0.01 --out new.csv | tee "$OUT/fit_gen$gen.txt"
python3 - <<'PY'
import csv, os
tot = {}
for path in ("cum.csv", "new.csv"):
    with open(path) as f:
        r = csv.reader(f); next(r)
        for leave, d in r: tot[leave] = tot.get(leave, 0.0) + float(d)
out = os.environ["OUT"] + "/cum.csv"
with open(out, "w") as f:
    f.write("leave,delta\n")
    for leave, d in sorted(tot.items()):
        if abs(d) >= 0.01: f.write(f"{leave},{d:.4f}\n")
new = {}
with open("new.csv") as f:
    r = csv.reader(f); next(r); new = {k: float(v) for k, v in r}
print("generation change: %d leaves moved, mean |change| %.3f, table entries %d" % (
    len(new), sum(abs(v) for v in new.values()) / max(1, len(new)), len(tot)))
PY
if ((gen < last)); then
  gh workflow run pruebas.yml -R "$repo" --ref "$GITHUB_REF_NAME" -f shards=1 -f seed=1 -f minutos=330 -f csw24=$csw \
    -f comando="scripts/scratch_gen.sh $lex $((gen + 1)) $last $GITHUB_RUN_ID $games"
  echo "generation $((gen + 1)) started"
fi
