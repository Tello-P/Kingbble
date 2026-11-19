ENABLE leave values built from scratch by Kingbble (no published leave table used, nothing from Woogles):
10 generations of full-rack equity self-play (scripts/scratch_gen.sh), every leave starting at 0.
Generations 1-5 ~80 M games each, 6-10 ~240 M; 8-10 with rare-rack forcing (FORCE='0.1 10') and
6-tile smoothing (SMOOTH6=1, kept only when it predicts the other data half better).
Mean change per leave: 7.59, 3.54, 1.53, 0.69, 0.36, ..., 0.344, 0.335, 0.060 (gen 10).
Full "leave,value" table, 914,624 leaves:
  gunzip -k configs/enable_leaves_gen10.csv.gz
  kingbble_selfplay --lexicon lexica/ENABLE.txt --leaves configs/enable_leaves_gen10.csv ...
Method check on CSW24 (same recipe, gen 10 vs the Woogles CSW24 table, sim 1 s + pre-endgame,
12,000 pairs): wins -0.24 +-0.54 pp (passes > -1 pp), points -0.50 +-0.96 per game (misses the
> -0.5 bound). Accepted by Tello on 2026-11-19.
