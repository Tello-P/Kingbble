#!/usr/bin/env bash
# Runs the per-issue scripts one after the other, shortest first, with an overall
# percentage weighted by each script's estimated time.
#   scripts/issues/run_all.sh [--dry-run] [--players nick1,nick2] [ISSUE...]
# With ISSUE numbers only those run. #38 needs --players (or KINGBBLE_PLAYERS)
# and is skipped without it.
set -euo pipefail
DIR=$(cd "$(dirname "$0")" && pwd)

# issue, estimated minutes, script
PLAN=(
  "31 20 issue_31_apeg_selfplay.sh"
  "33 45 issue_33_endgame_macondo_25.sh"
  "32 60 issue_32_endgame_macondo_full.sh"
  "30 360 issue_30_preendgame_macondo.sh"
  "34 1200 issue_34_quackle_and_smp.sh"
  "38 200 issue_38_collect_turns.sh"
  "47 205 issue_47_inference_eval.sh"
)

dry=() players=${KINGBBLE_PLAYERS:-} only=()
while (($#)); do
  case $1 in
    --dry-run) dry=(--dry-run) ;;
    --players) players=${2:?--players needs a value}; shift ;;
    [0-9]*) only+=("$1") ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

selected=()
for entry in "${PLAN[@]}"; do
  read -r issue _ _ <<<"$entry"
  if ((${#only[@]})) && [[ " ${only[*]} " != *" $issue "* ]]; then continue; fi
  if [[ $issue == 38 && -z $players && ${#dry[@]} -eq 0 ]]; then
    echo "[all] skipping #38: no --players given"
    continue
  fi
  selected+=("$entry")
done
total=0
for entry in "${selected[@]}"; do
  read -r _ minutes _ <<<"$entry"
  total=$((total + minutes))
done
((total)) || { echo "[all] nothing to run"; exit 0; }

done_minutes=0 k=0 failed=()
for entry in "${selected[@]}"; do
  read -r issue minutes script <<<"$entry"
  k=$((k + 1))
  echo "[all] $((100 * done_minutes / total))% - #$issue ($k/${#selected[@]}), about $minutes min"
  extra=()
  [[ $issue == 38 ]] && extra=(--players "${players:-<seed list>}")
  # A failing script (mismatches, missing setup) does not stop the others.
  if ! "$DIR/$script" "${dry[@]}" "${extra[@]}" | while IFS= read -r line; do
    echo "$line"
    if [[ $line =~ ^\[#$issue\]\ ([0-9]+)% ]]; then
      echo "[all] $(((100 * done_minutes + minutes * BASH_REMATCH[1]) / total))%"
    fi
  done; then
    failed+=("#$issue")
  fi
  done_minutes=$((done_minutes + minutes))
done
echo "[all] 100% - finished${failed[*]:+, failed: ${failed[*]}}"
((${#failed[@]} == 0))
