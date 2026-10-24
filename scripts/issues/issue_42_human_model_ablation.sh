#!/usr/bin/env bash
# #42: ablation of the human move model's feature groups on the val split.
#   1. the full model (logit, and with --mlp also the network);
#   2. one logit fit per feature group left out (A word, B missed bingos, C leave,
#      D shape, E defence, F ranking, G context, H interactions);
#   3. the val NLL of each against the full model, then, only at the end, the
#      full model on the test split.
# Each fit leaves <out>/<name>.json and is skipped on a rerun (resume).
# The features must exist (data/features with a version 2 header): the #47
# script builds them, or ml.data.word_freq and ml.data.features.
#
# --quick: 20,000 train / 10,000 val turns and one penalty per fit (about 10 min
# on 3 threads); the full run uses the logit defaults (60,000 / 20,000 turns,
# 9 penalties; about 15 min per fit). Resources as #47: --threads N, --mem SIZE,
# --force (see limits_init in lib.sh).
#   scripts/issues/issue_42_human_model_ablation.sh [--quick] [--mlp] [--dry-run]
#     [--threads N] [--mem SIZE] [--force]
source "$(dirname "$0")/lib.sh"

MODE=full MLP=0
FIT=()
FIT_UNITS=9000  # measured 0.1 s units per full fit
for a in "$@"; do
  case $a in
    --quick)
      MODE=quick FIT_UNITS=900
      FIT=(--max-train 20000 --max-val 10000 --search-train 0 --l2 0.1 --l1 0 --top-k 256)
      ;;
    --mlp) MLP=1 ;;
  esac
done
limits_init "$@"
busy_check 42

FEATURES=data/features
OUT=build/issue_runs/issue_42/$MODE
GROUPS_=(A B C D E F G H)
RUNS=(full "${GROUPS_[@]}")

DONE_UNITS=0
for name in "${RUNS[@]}"; do
  [[ -e $OUT/$name.json ]] && DONE_UNITS=$((DONE_UNITS + FIT_UNITS))
done
[[ -e $OUT/test.json ]] && DONE_UNITS=$((DONE_UNITS + FIT_UNITS))
issue_init 42 $(((${#RUNS[@]} + 1) * FIT_UNITS)) "$@"
need "$FEATURES/options.bin" "build the features first: scripts/issues/issue_47_inference_eval.sh"
if ((!DRY_RUN)) && [[ $(head -c 4 "$FEATURES/options.bin") != KBFT ]]; then
  echo "[#42] $FEATURES/options.bin is version 1: rebuild it with ml.data.features" >&2
  exit 1
fi
((DRY_RUN)) || mkdir -p "$OUT"
skip_units "$DONE_UNITS"
((DONE_UNITS)) && note "resuming in $MODE mode: $((100 * DONE_UNITS / TOTAL))% already done"

# fit NAME ARGS...: one model fit with a progress line per fitting stage.
fit() {
  local name=$1
  shift
  [[ -e $OUT/$name.json ]] && return
  # Penalties tried, then the bands and the mixture, then the network's two.
  local stages=11
  [[ $MODE == quick ]] && stages=3
  [[ " $* " == *" --mlp "* ]] && stages=$((stages + 2))
  STEP_SCALE=$((FIT_UNITS / stages)) step "$stages" "" "$PY" -m ml.human_model.logit \
    --features "$FEATURES" "${FIT[@]}" --out "$OUT/$name.txt" --report "$OUT/$name.json.tmp" \
    --progress "$@"
  ((DRY_RUN)) || mv "$OUT/$name.json.tmp" "$OUT/$name.json"
}

if ((MLP)); then fit full --mlp; else fit full; fi
for g in "${GROUPS_[@]}"; do fit "$g" --drop "$g"; done
((DRY_RUN)) && exit 0

summary() {
  "$PY" - "$OUT" "${GROUPS_[@]}" <<'EOF'
import json
import sys
from pathlib import Path

out, groups = Path(sys.argv[1]), sys.argv[2:]
full = json.loads((out / "full.json").read_text())
print(f"full: val NLL {full['val_nll_logit']:.4f} (logit), {full['features']} features, "
      f"top_k {full['top_k']}, coverage {full['coverage']}")
if "val_nll_mlp" in full:
    print(f"full: val NLL {full['val_nll_mlp']:.4f} (mlp), kept: {full['kind']}")
for g in groups:
    r = json.loads((out / f"{g}.json").read_text())
    delta = r["val_nll_logit"] - full["val_nll_logit"]
    print(f"without {g}: val NLL {r['val_nll_logit']:.4f} ({delta:+.4f}), {r['features']} features")
EOF
}
summary | while IFS= read -r line; do note "$MODE: $line"; done

# The test split, once, for the full model.
if [[ ! -e $OUT/test.json ]]; then
  if ((MLP)); then
    fit test --mlp --test
  else
    fit test --test
  fi
fi
"$PY" -c '
import json, sys
r = json.load(open(sys.argv[1]))
nll, n, cov = r["test_nll_logit"], r["test_turns"], r["test_coverage"]
line = f"test NLL {nll:.4f} (logit) on {n} turns, coverage {cov:.3f}"
if "test_nll_mlp" in r:
    mlp = r["test_nll_mlp"]
    line += f", {mlp:.4f} (mlp)"
print(line)
' "$OUT/test.json" | while IFS= read -r line; do note "$MODE: $line"; done
