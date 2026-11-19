#!/usr/bin/env bash
# Our own leave table for a lexicon (wolges census, self-play and blend), measured
# against the published one; at the lowest CPU priority. See train_leaves.py.
#   scripts/train_leaves.sh LEXICON_FILE fast|normal|max [--default-leaves KLV] [--out KLV]
exec nice -n 19 python3 "$(dirname "$0")/train_leaves.py" "$@"
