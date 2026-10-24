#!/usr/bin/env bash
# Runs the per-issue scripts one after the other, shortest first, with an overall
# percentage weighted by each script's estimated time.
#   scripts/issues/run_all.sh [--dry-run] [--quick] [--fresh] [--status] [--players LIST] [ISSUE...]
# Every finished game or position is kept, so stopping run_all.sh (Ctrl-C, or
# killing it) and starting it again goes on where it was; --fresh starts over and
# --status only reports how far each run is.
# --quick runs every script's bounded smoke check (see each script); it is not the
# final validation. #38 (data collection) has no quick mode and is left out.
# With ISSUE numbers only those run. #38 needs --players (or KINGBBLE_PLAYERS)
# and is skipped without it.
source "$(dirname "${BASH_SOURCE[0]}")/inhibit.sh"
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
  "54 400 issue_54_risk_eval.sh"
)

dry=() quick=() more=() status=0 players=${KINGBBLE_PLAYERS:-} only=()
while (($#)); do
  case $1 in
    --dry-run) dry=(--dry-run) ;;
    --quick) quick=(--quick) ;;
    --fresh) more+=(--fresh) ;;
    --status) more+=(--status) status=1 ;;
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
  if [[ $issue == 38 && ${#quick[@]} -gt 0 ]]; then
    echo "[all] skipping #38: no quick mode"
    continue
  fi
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
((${#quick[@]})) && echo "[all] QUICK SMOKE CHECK: bounded runs, not the final validation"

done_minutes=0 k=0 failed=()
for entry in "${selected[@]}"; do
  read -r issue minutes script <<<"$entry"
  k=$((k + 1))
  # Quick runs have not been timed yet: no per-step estimate.
  echo "[all] $((100 * done_minutes / total))% - #$issue ($k/${#selected[@]})$( ((${#quick[@]})) || echo ", about $minutes min")"
  extra=()
  [[ $issue == 38 ]] && extra=(--players "${players:-<seed list>}")
  # A failing script (mismatches, missing setup) does not stop the others.
  if ((status)); then
    "$DIR/$script" "${quick[@]}" "${more[@]}" "${extra[@]}" || true
    continue
  fi
  if ! "$DIR/$script" "${dry[@]}" "${quick[@]}" "${more[@]}" "${extra[@]}" | while IFS= read -r line; do
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
