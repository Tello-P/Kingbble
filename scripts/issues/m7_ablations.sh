#!/usr/bin/env bash
# M7: reproducible ablation tournament. One paired self-play run per experiment
# in configs/m7 (merged over configs/m7/common.yaml): opponent-rack inference on
# vs off, human move model vs RangeFinder, simulation vs static equity, endgame
# solver on vs off. Each run plays fixed simulation iterations with per-game
# seeds, so the same config and seed play the same games, and writes a manifest
# (commit, config hash, file hashes, seeds, threads, flags) next to its results.
# Every finished game is kept: stop the script any time and run it again to go
# on (--fresh starts over, --status reports, --dry-run prints the commands).
# --quick: 20 games per experiment with a light simulation (a smoke check of the
# pipeline, not a result). --only NAME runs one experiment. The report (markdown
# table, CSV and CI plot from ml.eval.tournament) goes to build/issue_runs/m7[_quick].
#   scripts/issues/m7_ablations.sh [--dry-run] [--quick] [--fresh] [--status] [--only NAME]
#     [--threads N] [--mem SIZE] [--force]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
CONFIG_DIR=configs/m7
ONLY=""
args=("$@")
for ((i = 0; i < ${#args[@]}; ++i)); do
  [[ ${args[i]} == --only ]] && ONLY=${args[i + 1]:?--only needs a value}
done
limits_init "$@"
QFLAG=()
((QUICK)) && QFLAG=(--quick)
cfg() { "$PY" -m ml.eval.tournament_config "$@"; }

CONFIGS=()
TOTAL_GAMES=0
while IFS= read -r c; do
  [[ -n $ONLY && $(basename "$c" .yaml) != "$ONLY" ]] && continue
  CONFIGS+=("$c")
  TOTAL_GAMES=$((TOTAL_GAMES + $(cfg games "$c" "${QFLAG[@]}")))
done < <(cfg list --dir "$CONFIG_DIR")
((${#CONFIGS[@]})) || {
  echo "[#m7] no experiment matches '${ONLY}' in $CONFIG_DIR" >&2
  exit 1
}

issue_init m7 "$TOTAL_GAMES" "$@"
((STATUS || DRY_RUN)) || busy_check m7
need_csw24
need models/human_model.txt "fit the human move model first (#67)"
build_engine

COMMIT=$(git rev-parse HEAD 2>/dev/null || echo unknown)
DIRTY=$([[ -z $(git status --porcelain --untracked-files=no 2>/dev/null) ]] && echo 0 || echo 1)
RESULTS=()
for c in "${CONFIGS[@]}"; do
  name=$(basename "$c" .yaml)
  mapfile -t RUN < <(cfg args "$c" "${QFLAG[@]}" --threads "$THREADS")
  games=$(cfg games "$c" "${QFLAG[@]}")
  out=$(results_file "$name")
  RESULTS+=("$out")
  ((STATUS)) || note "$name: $games games ($c)"
  step "$games" "" build/engine/kingbble_selfplay "${RUN[@]}" --progress --results "$out" \
    --tag commit="$COMMIT" --tag dirty="$DIRTY" --tag config="$c" \
    --tag config_hash="$(cfg hash "$c" "${QFLAG[@]}")" --tag mode="$( ((QUICK)) && echo quick || echo full)"
done
((DRY_RUN)) && exit 0
# Win % and spread per experiment with paired 95 % CIs and Holm-corrected p-values.
REPORT=build/issue_runs/m7$(quick_suffix)
if ((STATUS)); then
  "$PY" -m ml.eval.tournament "${RESULTS[@]}"
else
  "$PY" -m ml.eval.tournament "${RESULTS[@]}" --out "$REPORT" | tee -a "$LOG"
fi
