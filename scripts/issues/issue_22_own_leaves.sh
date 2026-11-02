#!/usr/bin/env bash
# #22: our own leave table for the development lexicon (ENABLE) against the
# published NWL23 leaves used since M2.
#   1. wolges' genleaves.sh (in ~/src/wolges, or --wolges DIR) plays its
#      autoplay generations on the ENABLE KWG and writes a KLV2 table;
#   2. paired self-play, equity with the new table (bot a, --a-leaves) against
#      equity with NWL23.klv2 (bot b), win % and spread with 95 % CIs over the
#      pairs. Check: the new table is not worse (win % CI reaching 50 or above).
# Full mode: 3 generations of 1,000,000 games and 2,000 paired games (the
# generations take many hours: about 10-30 h on 2 cores). --quick: 3
# generations of 20,000 games and 200 paired games (under an hour).
# --games-per-gen N and --games N change the sizes. Every step resumes: the
# generated table is kept, and the paired games keep their results file.
# Resources: --threads N (or KINGBBLE_THREADS, default 3) and --mem SIZE under
# lib.sh's LIMITS. It refuses to start while another heavy run is active, unless
# --force.
#   scripts/issues/issue_22_own_leaves.sh [--quick] [--dry-run] [--fresh] [--status]
#     [--games-per-gen N] [--games N] [--wolges DIR] [--threads N] [--mem SIZE] [--force]
source "$(dirname "$0")/lib.sh"

quick_args "$@"
GEN=1000000 GAMES=2000 WOLGES=$HOME/src/wolges
((QUICK)) && GEN=20000 GAMES=200
args=("$@")
for ((i = 0; i < ${#args[@]}; ++i)); do
  case ${args[i]} in
    --games-per-gen) GEN=${args[i + 1]:?} ;;
    --games) GAMES=${args[i + 1]:?} ;;
    --wolges) WOLGES=${args[i + 1]:?} ;;
  esac
done
limits_init "$@"
issue_init 22 $((3 * GEN / 1000 + GAMES)) "$@"
((STATUS || DRY_RUN)) || busy_check 22
need lexica/ENABLE.txt "python scripts/fetch_lexicon.py --preset enable"
need lexica/NWL23.klv2 "the published NWL23 leaves (see M2)"
need "$WOLGES/genleaves.sh" "clone https://github.com/andy-k/wolges to $WOLGES"
build_engine

OUT=$LOG_DIR/issue_22$(quick_suffix)
TABLE=$OUT/ENABLE_own.klv2
if [[ ! -e $TABLE ]]; then
  ((DRY_RUN || STATUS)) || mkdir -p "$OUT/gen"
  step 0 "" build/engine/kingbble_buildkwg lexica/ENABLE.txt "$OUT/gen/ENABLE.kwg"
  # genleaves.sh works in the current directory (one .kwg there), resumes from
  # the leavesN.klv2 it finds and leaves the last generation's table there.
  (cd "$OUT/gen" 2>/dev/null || true
    step $((3 * GEN / 1000)) "" "$WOLGES/genleaves.sh" --threads "$MAX_THREADS" \
      --preset english "$GEN" "$GEN" "$GEN")
  if ! finish; then
    klv=$(ls -t "$OUT"/gen/leaves*.klv2 2>/dev/null | head -1)
    [[ -n $klv ]] || { note "genleaves.sh left no leaves*.klv2 in $OUT/gen"; exit 1; }
    cp "$klv" "$TABLE"
  fi
else
  skip_units $((3 * GEN / 1000))
fi

step "$GAMES" "" build/engine/kingbble_selfplay --lexicon lexica/ENABLE.txt \
  --leaves lexica/NWL23.klv2 --a-leaves "$TABLE" --a equity --b equity --paired \
  --games "$GAMES" --seed 22 --progress --results "$(results_file paired)"
finish && exit 0
awk -v mode=own_leaves -v games="$GAMES" -f scripts/issues/paired_ci.awk \
  "$(results_file paired)" | sed 's/\[#54\]/[#22]/; s/ vs sim/ vs NWL23 leaves/' | tee -a "$LOG"
