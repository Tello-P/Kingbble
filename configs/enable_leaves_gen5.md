ENABLE leave values built from scratch by Kingbble (no published leave table used):
5 generations of full-rack self-play (scripts/scratch_gen.sh, ~80 M games each).
The table is every leave at 0 (scripts/zero_leaves.py) plus these "leave,delta" lines:
  gunzip -k configs/enable_leaves_gen5.csv.gz
  kingbble_selfplay --leaves zero.csv --leave-deltas configs/enable_leaves_gen5.csv ...
