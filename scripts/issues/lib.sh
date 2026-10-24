# Shared helpers for the per-issue run scripts (source it, don't run it).
#
# A script calls issue_init with its issue number, then step for each command.
# Every step's output goes to build/issue_runs/issue_<N>.log; the terminal gets
# the tool's own lines plus "[#N] P% (done/total) ETA ..." status lines, counted
# from "progress: D/T" lines printed by the tool or from lines matching a regex.

source "$(dirname "${BASH_SOURCE[0]}")/inhibit.sh"
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$ROOT"
LOG_DIR=$ROOT/build/issue_runs
# The repo's virtualenv has the data dependencies (duckdb, pyarrow).
PY=${PYTHON:-$([[ -x .venv/bin/python ]] && echo .venv/bin/python || echo python3)}
DRY_RUN=0
ARGS=()  # arguments left after the common ones
ISSUE=""
LOG=""
TOTAL=1        # work units over the whole script
DONE=0         # units finished by earlier steps
START_DONE=0   # units already done when this run started (resume), excluded from the ETA
START_TIME=$SECONDS
BUILD_PREFIX=()

# issue_init N TOTAL_UNITS "$@": parses --dry-run and opens the log.
issue_init() {
  ISSUE=$1
  TOTAL=$2
  shift 2
  for a in "$@"; do
    case $a in
      --dry-run) DRY_RUN=1 ;;
      *) ARGS+=("$a") ;;
    esac
  done
  LOG=$LOG_DIR/issue_$ISSUE.log
  if ((!DRY_RUN)); then
    mkdir -p "$LOG_DIR"
    echo "=== $(date '+%F %T') $(git rev-parse --short HEAD 2>/dev/null || echo '?')" >>"$LOG"
  fi
}

fmt_duration() {
  local s=$1
  if ((s >= 3600)); then
    printf '%dh%02dm' $((s / 3600)) $((s % 3600 / 60))
  elif ((s >= 60)); then
    printf '%dm' $((s / 60))
  else
    printf '%ds' "$s"
  fi
}

# status DONE: one status line; the ETA extrapolates this run's rate.
status() {
  local done=$1 pct=$((100 * $1 / TOTAL)) elapsed=$((SECONDS - START_TIME)) eta="?"
  if ((done > START_DONE && elapsed > 0)); then
    eta=$(fmt_duration $((elapsed * (TOTAL - done) / (done - START_DONE))))
  fi
  echo "[#$ISSUE] $pct% ($done/$TOTAL) ETA $eta"
}

# note TEXT: a line on the terminal and in the log.
note() {
  echo "[#$ISSUE] $*"
  ((DRY_RUN)) || echo "[#$ISSUE] $*" >>"$LOG"
}

# skip_units N: counts N units as already done (resumed work).
skip_units() {
  DONE=$((DONE + $1))
  START_DONE=$DONE
}

# step UNITS REGEX CMD...: runs CMD. Its "progress: D/T" lines (D of this step)
# or, with a non-empty REGEX, its lines matching REGEX advance the count; those
# lines only go to the log. Set STEP_LOG to also copy the output to that file,
# and STEP_SCALE to weigh each of the step's units as that many script units
# (so the percentage and ETA follow time when steps have units of unequal cost).
step() {
  local units=$1 regex=$2 scale=${STEP_SCALE:-1}
  shift 2
  if ((DRY_RUN)); then
    printf '%q ' "$@"
    echo
    DONE=$((DONE + units * scale))
    return
  fi
  local base=$DONE
  echo "\$ $*" | tee -a "$LOG"
  "$@" 2>&1 | {
    local n=0 shown=-1 shown_at=$SECONDS line pct
    while IFS= read -r line; do
      echo "$line" >>"$LOG"
      [[ -n ${STEP_LOG:-} ]] && echo "$line" >>"$STEP_LOG"
      if [[ $line =~ ^progress:\ ([0-9]+)/[0-9]+ ]]; then
        n=${BASH_REMATCH[1]}
      elif [[ -n $regex && $line =~ $regex ]]; then
        n=$((n + 1))
      else
        echo "$line"
        continue
      fi
      ((n > units)) && n=$units
      pct=$((100 * (base + n * scale) / TOTAL))
      # Print when the percentage moves or once a minute.
      if ((pct != shown || SECONDS - shown_at >= 60)); then
        status $((base + n * scale))
        shown=$pct
        shown_at=$SECONDS
      fi
    done
  }
  DONE=$((base + units * scale))
  status "$DONE" | tee -a "$LOG"
}

# limits_init "$@": resources of heavy steps from --threads N (or KINGBBLE_THREADS,
# default 3), --mem SIZE (or KINGBBLE_MEM_MAX, default 6G, "none" to disable) and
# --force. Sets THREADS (also for BLAS/OpenMP), FORCE, HEAVY (the command prefix
# for heavy steps: lowest priority and, with a memory cap, a systemd user scope
# with MemoryMax and no swap, else ulimit -v), BUILD_JOBS and BUILD_PREFIX.
limits_init() {
  local args=("$@") i kb
  THREADS=${KINGBBLE_THREADS:-3} MEM=${KINGBBLE_MEM_MAX:-6G} FORCE=0
  for ((i = 0; i < ${#args[@]}; i++)); do
    case ${args[i]} in
      --threads) THREADS=${args[i + 1]:-} ;;
      --mem) MEM=${args[i + 1]:-} ;;
      --force) FORCE=1 ;;
    esac
  done
  export OMP_NUM_THREADS=$THREADS OPENBLAS_NUM_THREADS=$THREADS MKL_NUM_THREADS=$THREADS \
    NUMEXPR_NUM_THREADS=$THREADS
  BUILD_JOBS=$THREADS
  HEAVY=(nice -n 19)
  if [[ $MEM != none ]]; then
    if systemd-run --user --scope --quiet -p MemoryMax="$MEM" -p MemorySwapMax=0 true 2>/dev/null; then
      HEAVY=(systemd-run --user --scope --quiet -p MemoryMax="$MEM" -p MemorySwapMax=0 nice -n 19)
    else
      kb=$(numfmt --from=iec "$MEM")
      HEAVY=(bash -c "ulimit -v $((kb / 1024)); exec nice -n 19 \"\$@\"" heavy)
    fi
  fi
  BUILD_PREFIX=("${HEAVY[@]}")
}

# busy_check N: exits unless --force (limits_init) when another issue script or
# engine binary runs (the machine runs out of memory with several heavy jobs).
# No subshells here: they would show up with this script's command line.
busy_check() {
  local pat='^([^ ]*/)?(ba)?sh [^ ]*issue_[0-9]+_[^ ]*\.sh|^[^ ]*(kingbble_[a-z_]+|engine_tests|macondo)( |$)'
  local list pid cmd others=""
  list=$(mktemp)
  pgrep -af -- "$pat" >"$list" || true
  while read -r pid cmd; do
    [[ $pid == "$$" || $pid == "$PPID" ]] && continue
    others+="  $pid $cmd"$'\n'
  done <"$list"
  rm -f "$list"
  if [[ -n $others ]] && ((!FORCE)); then
    echo "[#$1] another heavy run is active; wait for it or pass --force:" >&2
    printf '%s' "$others" >&2
    exit 1
  fi
}

# need FILE HINT: fails unless FILE exists (not checked on a dry run).
need() {
  if ((!DRY_RUN)) && [[ ! -e $1 ]]; then
    echo "[#$ISSUE] missing $1: $2" >&2
    exit 1
  fi
}

# need_csw24: the CSW24 word list and leaves the strength runs play with. Both are
# licensed, so they live in lexica/ (ignored by git):
#   build/quackle_helper/quackle_helper --lexicon csw24 --words > lexica/CSW24.txt
#   curl -o lexica/CSW24.klv2 \
#     https://raw.githubusercontent.com/woogles-io/liwords/master/liwords-ui/public/wasm/2024/CSW24.klv2
need_csw24() {
  need lexica/CSW24.txt "export it with: quackle_helper --lexicon csw24 --words > lexica/CSW24.txt"
  need lexica/CSW24.klv2 "download CSW24.klv2 from woogles-io/liwords (see scripts/issues/lib.sh)"
}

# build_engine: configures and builds the engine (quick when up to date), with
# BUILD_JOBS jobs (default 8) under the BUILD_PREFIX command array (e.g. nice).
build_engine() {
  local jobs=${BUILD_JOBS:-8}
  if ((DRY_RUN)); then
    echo "cmake -S . -B build && cmake --build build -j$jobs"
    return
  fi
  cmake -S . -B build >>"$LOG" 2>&1
  "${BUILD_PREFIX[@]}" cmake --build build -j"$jobs" >>"$LOG" 2>&1
}
