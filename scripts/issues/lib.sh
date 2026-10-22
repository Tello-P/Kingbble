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
# Load limits for every run, so the PC stays usable: at most MAX_THREADS busy
# cores (jobs x threads, helper processes included) and MAX_MEM of memory with no
# swap, at the lowest CPU and I/O priority.
MAX_THREADS=${KINGBBLE_MAX_THREADS:-3}
MAX_MEM=${KINGBBLE_MAX_MEM:-6G}
LIMITS=(nice -n 19 ionice -c3)
if command -v systemd-run >/dev/null && systemd-run --user --scope --quiet true 2>/dev/null; then
  LIMITS=(systemd-run --user --scope --quiet -p MemoryMax="$MAX_MEM" -p MemorySwapMax=0
    -p CPUQuota=$((100 * MAX_THREADS))% "${LIMITS[@]}")
fi
DRY_RUN=0
FRESH=0   # --fresh: forget the results of earlier runs and start over
STATUS=0  # --status: only report how far the run is, from its results files
QUICK=0   # --quick: a bounded smoke check, not the final validation
ARGS=()  # arguments left after the common ones
ISSUE=""
LOG=""
TOTAL=1        # work units over the whole script
DONE=0         # units finished by earlier steps
START_DONE=0   # units already done when this run started (resume), excluded from the ETA
START_TIME=$SECONDS

# quick_suffix [TEXT]: TEXT (default "_quick") in quick mode, nothing otherwise.
quick_suffix() {
  if ((QUICK)); then echo "${1:-_quick}"; fi
}

# quick_args "$@": sets QUICK when --quick is among the arguments, so a script
# can size its work before issue_init.
quick_args() {
  local a
  for a in "$@"; do [[ $a == --quick ]] && QUICK=1; done
  return 0
}

# issue_init N TOTAL_UNITS "$@": parses --dry-run and --quick and opens the log.
# Quick runs log to issue_<N>_quick.log and say so at the top.
issue_init() {
  ISSUE=$1
  TOTAL=$2
  shift 2
  for a in "$@"; do
    case $a in
      --dry-run) DRY_RUN=1 ;;
      --quick) QUICK=1 ;;
      --fresh) FRESH=1 ;;
      --status) STATUS=1 ;;
      *) ARGS+=("$a") ;;
    esac
  done
  LOG=$LOG_DIR/issue_$ISSUE$(quick_suffix).log
  if ((STATUS)); then
    DRY_RUN=0
    echo "[#$ISSUE] status$(quick_suffix " (quick)")"
    return
  fi
  if ((!DRY_RUN)); then
    mkdir -p "$LOG_DIR"
    echo "=== $(date '+%F %T') $(git rev-parse --short HEAD 2>/dev/null || echo '?')" >>"$LOG"
  fi
  note "limits: $MAX_THREADS cores, $MAX_MEM memory (KINGBBLE_MAX_THREADS, KINGBBLE_MAX_MEM)"
  if ((QUICK)); then
    note "QUICK SMOKE CHECK: bounded, slow positions skipped. Not the final validation."
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

# results_file NAME: where a step keeps its finished games or positions, so that
# a stopped run goes on from there when started again (--fresh deletes it first).
results_file() {
  local f=$LOG_DIR/results/issue_${ISSUE}_$1$(quick_suffix).txt
  if ((FRESH && !DRY_RUN && !STATUS)); then rm -f "$f"; fi
  ((DRY_RUN)) || mkdir -p "$LOG_DIR/results"
  echo "$f"
}

# step UNITS REGEX CMD...: runs CMD. Its "progress: D/T" lines (D of this step)
# or, with a non-empty REGEX, its lines matching REGEX advance the count; those
# lines only go to the log. Set STEP_LOG to also copy the output to that file.
step() {
  local units=$1 regex=$2
  shift 2
  if ((STATUS)); then
    # Every tool that keeps a results file reports on it with --status.
    if [[ " $* " == *" --results "* ]]; then "$@" --status 2>/dev/null || true; fi
    return
  fi
  if ((DRY_RUN)); then
    printf '%q ' "$@"
    echo
    DONE=$((DONE + units))
    return
  fi
  local base=$DONE
  echo "\$ $*" | tee -a "$LOG"
  "${LIMITS[@]}" "$@" 2>&1 | {
    local n=0 shown=-1 shown_at=$SECONDS line pct first=1
    while IFS= read -r line; do
      echo "$line" >>"$LOG"
      [[ -n ${STEP_LOG:-} ]] && echo "$line" >>"$STEP_LOG"
      if [[ $line =~ ^progress:\ ([0-9]+)/[0-9]+ ]]; then
        n=${BASH_REMATCH[1]}
        # A resumed tool reports its starting point first; keep it out of the ETA.
        ((first && n > 1)) && START_DONE=$((base + n))
        first=0
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

# finish: true when the script should stop after its steps (status or dry run).
finish() { ((DRY_RUN || STATUS)); }

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
  ((STATUS)) && return
  if ((DRY_RUN)); then
    echo "cmake -S . -B build && cmake --build build -j$MAX_THREADS"
    return
  fi
  "${LIMITS[@]}" cmake -S . -B build >>"$LOG" 2>&1
  "${LIMITS[@]}" cmake --build build -j"$MAX_THREADS" >>"$LOG" 2>&1
}
