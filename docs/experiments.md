# Experiments behind the thesis tables

One command per table. Run everything from the repo root after building the engine
(`cmake -S . -B build && cmake --build build -j2`). Every script below:

- keeps each finished game or position in `build/issue_runs/results/`, so a stopped run
  continues when the same command is started again (`--fresh` starts over);
- reports progress with `--status` and prints its commands with `--dry-run`;
- offers `--quick`, a bounded smoke check of the pipeline (not a result);
- runs at the lowest CPU and I/O priority with at most `KINGBBLE_MAX_THREADS` cores
  (the times below are for `KINGBBLE_MAX_THREADS=2`).

CSW24 (`lexica/CSW24.txt`, `lexica/CSW24.klv2`) is licensed and stays out of git;
`scripts/issues/lib.sh` (`need_csw24`) says how to get it. The human move model is
`models/human_model.txt` (#67).

All paired runs play each deal twice with the seats swapped, so the luck of the draw
cancels out within a pair. `python -m ml.eval.tournament RESULTS...` turns any paired
self-play results file into win % and spread with paired 95 % CIs.

## Ablations (M7)

The full bot `sim+inf:human+eg` against itself without one component, 400 paired games
each, fixed simulation iterations, per-game seeds and a depth-limited endgame, so the
same config replays the same games:

| Experiment | Bot b | Question |
|---|---|---|
| `inference` | `sim+inf:none+eg` | does tracking the opponent's rack help? |
| `human_vs_rangefinder` | `sim+inf:rf+eg` | human move model or RangeFinder? |
| `sim_vs_equity` | `equity+eg` | what does simulation add? |
| `endgame` | `sim+inf:human` | what does the endgame solver add? |

```bash
KINGBBLE_MAX_THREADS=2 scripts/issues/m7_ablations.sh [--only NAME]
```

Configs: `configs/m7/` (`common.yaml` plus one file per experiment). Each results file
has a `.manifest.json` beside it (commit, config hash, lexicon/leaves/model hashes,
seeds, threads, flags). Report: `build/issue_runs/m7/tournament.{md,csv,svg}`, with
p-values Holm-corrected across the four experiments. About 22 h for the four
(`--only NAME` runs one, about 5-6 h).

## Quackle match (#34)

`sim+peg+eg` against Quackle's Championship Player on CSW24; v0.2 exit criterion:
Kingbble win % >= 45.

```bash
KINGBBLE_MAX_THREADS=2 scripts/issues/issue_34_quackle_and_smp.sh --match-only --games 300 [--sim-time 5]
```

`--sim-time S` simulates S seconds per move (as a timed match) and keeps its own results
file, apart from the fixed-iteration games. Builds the Quackle helper from
`~/src/quackle` on first use. About 30 h for 300 games. Without `--match-only` it also
runs the Lazy SMP endgame check (several threads vs one).

## Macondo match (#60)

`sim+peg+eg` against Macondo's simulating bot on CSW24 at equal time per move and equal
threads, 300 paired games.

```bash
KINGBBLE_MAX_THREADS=2 scripts/issues/issue_58_macondo_match.sh --time 5
```

Needs a Macondo checkout in `~/src/macondo` and Go. About 10 h on 3 threads.
Last result (2 cores, 5 s per move): 47.0 % (95 % CI 41.7-52.3), spread +4.8
(-6.3..+15.9).

## Rack inference (#47)

Opponent-rack inference on CSW24: log-loss and accuracy on held-out human turns (none,
RangeFinder, human model), self-play of human inference against RangeFinder, and the
stress test against a rival playing rare moves.

```bash
KINGBBLE_MAX_THREADS=2 scripts/issues/issue_47_inference_eval.sh
```

Needs the Woogles games (`data/raw/woogles`, #38) and their turn store. About 3.5 h
(`--quick` about 1 h 10). Metrics: `ml/eval/metrics.py`.

## Endgame solver vs equity (#48)

`equity+eg` against `equity`, 200 paired games per seed at 1 s per endgame, with the
solver's statistics (solves, exact solves, moves changed):

```bash
for seed in 1 2 3; do
  nice -n 19 build/engine/kingbble_selfplay --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 \
    --a equity+eg --b equity --paired --games 200 --seed $seed --threads 2 --endgame-time 1 \
    --endgame-stats --progress --results build/issue_runs/results/issue_48_s$seed.txt
done
python -m ml.eval.tournament build/issue_runs/results/issue_48_s*.txt
```

Last result (600 paired games): +4.8 spread per game (95 % CI +4.0..+5.6), 51.4 % wins
(50.4-52.5); about 80 % of endgames solved exactly at 1 s.

## Self-play leaves and bot model (#49)

1. Self-play games (equity vs equity) and the bot move model fitted on them (about
   3 h and 42 GB of disk in all; `--estimate` prints the figures and exits):

   ```bash
   python -m ml.train --games 45000 --source selfplay --bots equity,equity --movers bot --threads 2
   ```

2. Leave values fitted on those games (`LEAVE,value` lines the engine reads as a leave
   table):

   ```bash
   python -m ml.rl.leave_fit --raw build/m6/data/equity_vs_equity_s1/gcg --out build/issue_runs/leaves_fit.csv
   ```

3. The fitted table (bot a) against CSW24.klv2 (bot b), 2,000 paired games (about
   10 min):

   ```bash
   nice -n 19 build/engine/kingbble_selfplay --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 \
     --a-leaves build/issue_runs/leaves_fit.csv --a equity --b equity --paired --games 2000 \
     --seed 2 --threads 2 --progress --results build/issue_runs/results/issue_49_compare.txt
   python -m ml.eval.tournament build/issue_runs/results/issue_49_compare.txt
   ```
