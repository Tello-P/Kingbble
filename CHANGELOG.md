# Changelog

Milestones of the bachelor's thesis. Each section lists what the milestone added.

## [1.0.0] - unreleased

Tagged once the thesis runs are done (`docs/experiments.md`).

### Added (M7: reproducible tournaments and ablations)
- `sim+inf:human|rf|none` bots: a sim bot that draws its simulated opponent racks
  from a rack filter on the human move model or RangeFinder, in `kingbble_selfplay`.
  The rack tracker is shared with `kingbble_inference_eval`.
- Reproducible runs: `--per-game-seeds` (a game depends only on the run's seed, the
  deal and the seat, so a resumed run replays the same games), a depth-limited endgame
  (`--endgame-plies`, `--endgame-threads`) and a manifest next to each results file
  (commit, config, file hashes, seeds, threads, flags). Resuming with another config
  is refused.
- The four ablations in `configs/m7/` and `scripts/issues/m7_ablations.sh`.
- `ml.eval.tournament`: paired win % and spread with 95 % CIs, Holm correction across
  experiments, markdown/CSV tables and a CI plot.
- `--sim-time` for the Quackle match script (#34).
- Determinism tests, the README and `docs/experiments.md`.

## M6: training menu and learned components

- Training menu (CLI and Streamlit) with data levels 0-3, a gate between model
  versions, MLflow tracking, the Auto loop (budgets, automatic k, stops, active model
  and revert) and compact features for the large levels.
- Human and self-play data mix, a LightGBM move model.
- Learned risk policy for RiskBot (#53).
- Leave values fitted on self-play games, leave tables read from CSV, bot a with its own
  leaves (#49), own ENABLE leave table against NWL23 (#22).
- Human move model trained on real human games only (#67).
- Endgame solver statistics and the #48 diagnosis (no bug: +4.8 spread per game).
- Time per simulated move (#56), pre-endgame speed-ups and time breakdowns (#78, #79).

## M5: risk-sensitive move selection

- RiskBot: mean-variance and win-probability selection among simulated candidates.
- Simulation settings (playout depth, playouts to the end near the end of the game).
- Match harness against Macondo (#58, #60).

## M4: opponent-rack inference

- Human move model (conditional logit by rating band, optional network) fitted on
  Woogles games, with word features.
- Rack filter (particles, exact tracking with few racks, surprise widening), RangeFinder
  baseline, rack sampler for the sim bot.
- `kingbble_inference_eval`: log-loss, accuracy and calibration on human turns, and
  self-play with inference (#47).

## M3: data

- GCG parser, Woogles downloader, turn store in Parquet queried with DuckDB,
  pseudonymized players, train/val/test split by player, memmap export.
- CSW24 as the target lexicon; resumable per-issue run scripts with a quick mode and
  resource limits.

## M2: search

- Leave values (KLV/KLV2), static equity, equity bot, Monte Carlo 2-ply simulation bot.
- Endgame solver (iterative deepening, transposition table, Lazy SMP, lexicon pruning),
  exact pre-endgame solver and approximate pre-endgame for 3-6 tiles in the bag.
- Differential tests against Macondo and a match harness against Quackle.

## M1: engine

- Tiles, racks, bag, board with premium squares, GADDAG and KWG move generation, game
  loop under NASPA rules, greedy self-play.

## M0: scoping

- Repo, CI, lexicon tooling, project proposal.
