title: Results
nav: Results
blurb: Every number of the project in one place: correctness, prediction, games.
lead: Every measurement of the project in one place, grouped by the question it answers: is the engine correct, does inference predict better, and does any of it win more games.
---
## Is the engine correct?

Before measuring strength, the engine has to be right. Positions were exported, handed to Macondo, and compared move by move and value by value.

:::bars min=0 max=100 unit=% caption=Exact agreement with Macondo. Generator: 10,000 positions with each of two seeds. Endgame: positions both solvers finish within 600 s. Pre-endgame: wins and total spread of the best moves over every ordering of the bag.
Move generator | 100 | | | 20,000 of 20,000 positions
Exact endgame | 100 | | | 34 of 34 positions
Pre-endgame, 1 tile in the bag | 100 | | | 28 of 28 moves
Pre-endgame, 2 tiles in the bag | 95.8 | | | 23 of 24 moves
:::

The one difference is a single move in a single position, 18 points of total spread over all orderings with the same wins, and it is under investigation. Details in [pre-endgame](preendgame).

One more fact from the endgame comparison: within the limit of 600 seconds, Kingbble's solver left 5 positions unfinished and Macondo's 11.

## Does inference predict better?

How much probability each method puts on the tiles the player really kept. Compare within a panel only: the two panels are different sets of turns.

:::bars min=0 max=13 caption=2,000 held-out human turns. Log-loss in nats per turn, lower is better.
Kingbble, human model | 5.74 | | | 
No inference | 5.87 | | | 
RangeFinder | 12.50 | | | 
:::

:::bars min=0 max=13 caption=1,000 turns played by bots, with the bot model.
Kingbble, bot model | 4.46 | | | 
No inference | 5.40 | | | 
RangeFinder | 9.08 | | | 
:::

And the move model on its own, predicting the move a person makes:

:::bars min=0 max=7 caption=Negative log-likelihood on test players, lower is better.
Human model (MLP, real games only) | 3.62 | | | 
Uniform choice among legal options | 6.38 | | | 
:::

Against not inferring, the gain with people is small (0.13 nats per turn). The large difference is against RangeFinder, which is too sharp. With bots, which are more predictable, the gain over not inferring is much larger (0.94 nats). Per-tile probabilities are well calibrated, with an error of about 1 %. More in [rack inference](inference).

## Does it win more games?

Each row is a separate experiment with its own opponent.

:::bars min=38 max=70 ref=50 unit=% caption=Win percentage with 95 % intervals. Rows in gold exclude 50 %. The dashed line is 50 %.
Inference: own model against RangeFinder | 58.5 | 51.7 | 65.3 | 200 games
Exact endgame against equity | 51.4 | 50.4 | 52.5 | 600 paired games
Approximate + exact pre-endgame | 51.5 | 49.3 | 53.7 | 400 paired games
Simulation, 3 plies against 2 | 51.0 | 47.5 | 54.5 | 200 games, corrected bank
Kingbble against Macondo, 5 s per move | 47.0 | 41.7 | 52.3 | 300 paired games
:::

- **Inference against RangeFinder: 117 of 200 games**, significantly above 50 %. The central result.
- **Exact endgame**: few extra games (+1.4 points), because it only decides close ones, but a clear effect in spread, below.
- **Pre-endgame and three plies**: neutral. They do not hurt; no gain is detected.
- **Against Macondo**: a statistical tie. More close games lost than won (17 against 27), which fits the last turns being the part still to polish. See [references](references).

## What the endgame adds in points

:::bars min=-3 max=7 ref=0 caption=Average spread per game over a bot with static equity only (the last row: over a bot with the exact endgame only). Paired games, 95 % intervals.
Exact endgame, 1 s | 4.8 | 4.0 | 5.6 | 600 games
Exact endgame, 10 s | 5.1 | 4.3 | 6.0 | 600 games
1 s, equity's move if not exact | 2.7 | 2.0 | 3.3 | 600 games
Approximate + exact pre-endgame | 0.5 | -2.0 | 2.9 | 400 games
:::

## Under stress

An opponent who, 30 % of the time, plays a random move outside their ten best.

:::bars min=60 max=100 unit=% caption=Win percentage against the erratic opponent, 100 games per side. The difference is not significant (±9 points); the point is that inference with the human model does not get worse.
Kingbble's model | 89 | | | +140 spread
RangeFinder | 85 | | | +128 spread
:::

## The three hypotheses

| Hypothesis | Verdict | Evidence |
|---|---|---|
| H1. A model learned from people predicts the real move better | **Confirmed** | NLL 3.62 against 6.38 on unseen players |
| H2. Using it in a multi-turn filter improves play | **Confirmed against RangeFinder**, modest against no inference | log-loss 5.74 against 12.50 and 5.87; 58.5 % of games |
| H3. A learned risk policy improves on the average | **Not shown** | every risk setting between 48 and 51.5 % |

## The four ablations

The final evaluation takes the full bot, `sim+inf:human+eg`, and plays it against itself with one part removed. 400 paired games each, a fixed number of simulation iterations, per-game seeds and a depth-limited endgame, so the same configuration replays the same games. The p-values are Holm-corrected across the four.

| Experiment | The other bot | The question |
|---|---|---|
| `inference` | `sim+inf:none+eg` | does tracking the opponent's rack help? |
| `human_vs_rangefinder` | `sim+inf:rf+eg` | human model or RangeFinder? |
| `sim_vs_equity` | `equity+eg` | what does simulation add? |
| `endgame` | `sim+inf:human` | what does the endgame solver add? |

The code and the configurations are finished. The runs take about 22 hours of machine time and are the last step before version 1.0 is tagged: see the [roadmap](roadmap). When they finish, their table goes here.

## Reproducing a number

Each table on this page is one command, listed in `docs/experiments.md` in the repository. Every script keeps each finished game on disk, so a stopped run continues where it left off; has a `--quick` mode that checks the pipeline in minutes; and runs at the lowest priority with a set number of cores. For example, the endgame diagnosis:

```
for seed in 1 2 3; do
  build/engine/kingbble_selfplay --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 \
    --a equity+eg --b equity --paired --games 200 --seed $seed --threads 2 \
    --endgame-time 1 --endgame-stats --results issue_48_s$seed.txt
done
python -m ml.eval.tournament issue_48_s*.txt
```
