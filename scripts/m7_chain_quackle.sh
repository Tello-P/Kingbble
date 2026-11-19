#!/usr/bin/env bash
# Waits for the Quackle level check (a "Long runs" run whose shards 0-1 played
# Championship Player and 2-3 Five Minute Championship Player), measures minutes
# per game and Kingbble's win % per setting, and starts the full 300-game match
# against the strongest setting that fits in one run of at most 20 shards.
#   scripts/m7_chain_quackle.sh CHECK_RUN_ID
set -euo pipefail
CHECK=${1:?check run id}
REPO=${GITHUB_REPOSITORY:-Tello-P/Kingbble}
OUT=${OUT:-.}
GAMES=300 MAX_SHARDS=20 MINUTES=330
# Seconds a shard has for games: the job limit minus the workflow's stop margin and setup.
BUDGET=$((MINUTES * 60 - 600 - 900))

until [[ $(gh run view "$CHECK" -R "$REPO" --json status -q .status) == completed ]]; do
  sleep 120
done
dir=$OUT/check
rm -rf "$dir"
gh run download "$CHECK" -R "$REPO" -D "$dir" --pattern 'shard-*'
jobs=$(gh api "repos/$REPO/actions/runs/$CHECK/jobs?per_page=100")

declare -A games secs wins
for s in 0 1 2 3; do
  d=$dir/shard-$s
  [[ -f $d/player.txt && -f $d/start.txt ]] || continue
  p=$(<"$d/player.txt")
  n=$(grep -sc '^game [0-9]*:' "$d/match.txt" || true)
  w=$(awk '/^game [0-9]*:/ { if ($4 > $7) w++ } END { print w + 0 }' "$d/match.txt" 2>/dev/null || echo 0)
  if [[ -f $d/end.txt ]]; then
    end=$(<"$d/end.txt")
  else
    end=$(jq -r --arg n "Shard $s" '.jobs[] | select(.name == $n) | .completed_at' <<<"$jobs")
    end=$(date -d "$end" +%s)
  fi
  t=$((end - $(<"$d/start.txt")))
  games[$p]=$((${games[$p]:-0} + ${n:-0}))
  secs[$p]=$((${secs[$p]:-0} + t))
  wins[$p]=$((${wins[$p]:-0} + w))
done

{
  echo "Quackle level check, run $CHECK"
  for p in "${!games[@]}"; do
    g=${games[$p]}
    if ((g > 0)); then
      echo "$p: $g games, $((secs[$p] / g / 60)) min per game, Kingbble won ${wins[$p]}"
    else
      echo "$p: no finished games"
    fi
  done
} | tee "$OUT/decision.txt"

choice="" shards=0 per=0
for p in "Five Minute Championship Player" "Championship Player"; do
  g=${games[$p]:-0}
  ((g > 0)) || continue
  per_game=$((secs[$p] * 11 / 10 / g + 1))
  fit=$((BUDGET / per_game))
  ((fit > 0)) || continue
  need=$(((GAMES + fit - 1) / fit))
  if ((need <= MAX_SHARDS)); then
    choice=$p shards=$MAX_SHARDS
    per=$(((GAMES + shards - 1) / shards))
    break
  fi
done
if [[ -z $choice ]]; then
  echo "no setting fits $GAMES games in $MAX_SHARDS shards; the match was not started" | tee -a "$OUT/decision.txt"
  exit 1
fi
echo "match: $choice, $shards shards x $per games" | tee -a "$OUT/decision.txt"

cmd="build/engine/kingbble_quackle_match --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2"
cmd+=" --helper build/quackle_helper/quackle_helper --bot sim+peg+eg --threads 2 --quackle-threads 2"
cmd+=" --quackle-player \"$choice\" --games $per --seed \$SEED --sim-time 5 --sim-iterations 5000"
cmd+=" --gcg-dir \$OUT/gcg --results \$OUT/match.txt"
gh workflow run pruebas.yml -R "$REPO" --ref "${GITHUB_REF_NAME:-testing/m7-experiments}" \
  -f comando="$cmd" -f shards="$shards" -f seed=34100 -f minutos="$MINUTES" \
  -f quackle=true -f csw24=true
echo "match started" | tee -a "$OUT/decision.txt"
