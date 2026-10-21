# Shared helpers for the per-issue run scripts (source it, don't run it).
#
# A script calls issue_init with its issue number, then step for each command.
# Every step's output goes to build/issue_runs/issue_<N>.log; the terminal gets
# the tool's own lines plus "[#N] P% (done/total) ETA ..." status lines, counted
# from "progress: D/T" lines printed by the tool or from lines matching a regex.

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
# lines only go to the log. Set STEP_LOG to also copy the output to that file.
step() {
  local units=$1 regex=$2
  shift 2
  if ((DRY_RUN)); then
    printf '%q ' "$@"
    echo
    DONE=$((DONE + units))
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
      pct=$((100 * (base + n) / TOTAL))
      # Print when the percentage moves or once a minute.
      if ((pct != shown || SECONDS - shown_at >= 60)); then
        status $((base + n))
        shown=$pct
        shown_at=$SECONDS
      fi
    done
  }
  DONE=$((base + units))
  status "$DONE" | tee -a "$LOG"
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

# build_engine: configures and builds the engine (quick when up to date).
build_engine() {
  if ((DRY_RUN)); then
    echo "cmake -S . -B build && cmake --build build -j8"
    return
  fi
  cmake -S . -B build >>"$LOG" 2>&1
  cmake --build build -j8 >>"$LOG" 2>&1
}
