title: How it is measured
nav: Measuring
blurb: Paired games, confidence intervals, independent seeds and a null test. Why two of the project's "findings" were noise.
lead: In a game with this much luck, measuring is the hard part. Paired games, confidence intervals, independent seeds, reproducible tournaments and the automatic tests that keep the bench itself honest.
---
## The problem with luck

Two bots play 200 games and one wins 104. Is it better?

No way to tell. With 200 independent games the 95 % interval of the win percentage is about ±7 points: 52 % is compatible with anything from 45 to 59. And the differences worth finding in this project are small. An exact endgame is worth about 1.5 points of win percentage. A good idea is worth 2 or 3.

To see 2 points with independent games you would need about 5,000 of them per comparison, and a game with simulation takes around 30 seconds on the hardware this project ran on: a personal computer with two or three cores, at low priority. So the method had to get sharper, not just bigger.

## Paired games

The biggest source of luck is the draw: who gets the blanks, the `S`s, the `Q`. Paired games remove it.

**Each deal is played twice with the seats swapped.** Same bag order, same starting racks. In the first game bot A sits in seat 1 and gets those tiles; in the second, bot B does. If A wins both, or wins by more than it loses by, the difference comes from decisions, because the tiles were the same.

The statistics are then computed **over pairs**, not over games. The interval comes from how much the pair results vary, which is far less than single games do.

| Experiment | Unpaired, 200 games | Paired |
|---|---|---|
| Exact endgame against equity | "47 %", ±7 points: looks like a bug | 51.4 % (50.4–52.5) and +4.8 spread (+4.0 to +5.6) over 600 games |
| Approximate pre-endgame | 44 % or 60 % depending on the seed | 51.5 % (49.3–53.7) over 400 games |

The first row is the story told in the [endgame chapter](endgame): a suspected bug that was noise. The second is the same lesson a week later, caught in time.

## Always an interval

Every number in this manual that comes from games has a 95 % confidence interval next to it. A result is called **significant** when the interval excludes "no difference": 50 % for win percentage, zero for spread. In the charts those rows are drawn in gold.

A result whose interval includes "no difference" is reported as *neutral*, not as a small win. Several features of the engine are honestly neutral: the approximate pre-endgame, three-ply playouts, every risk setting.

## Independent seeds

Each batch of games is started from a seed. In the first version game `g` used `seed + g`, so batches with seed 1 and seed 2 shared 199 of their 200 games. Two "independent" runs agreed with each other suspiciously well, 109 to 91 both times.

Now every game's seed is mixed with SplitMix64. And with `--per-game-seeds` a game depends only on the run's seed, the deal and the seat, never on which games were played before it. A run that is stopped and resumed replays exactly the same games.

## The null test

The most useful test in the project compares a bot **with itself**.

Two identical bots playing a pair of games, same deal with the seats swapped, must split it one win each, every time. If they do not, the bench is adding noise that pairing was supposed to cancel.

This test did not exist when a position bank reported that three-ply playouts won 56.5 % against two-ply ones. The bank's random numbers were tied to *the bot* (A or B) and not to *the seat*. With identical bots a pair did not always come out one-all, and the leftover noise repeated in every test run on that bank. With the random streams tied to the seat, the same comparison gives 51.0 % (47.5–54.5). The improvement was an artefact.

The null test now runs automatically.

## A bank of positions

Whole games spend most of their time in ordinary positions where every sensible bot plays the same move. To study one phase, the project uses a **position bank**: positions taken from games at a chosen stage (for example, 10 to 25 tiles in the bag and a score within 80 points), each played out to the end twice, once from each side. It concentrates the games where they can show a difference.

For positions near the end there is also an **exact judge**: with a small bag, two candidate moves can be compared by solving the position exactly rather than by playing it out.

## Reproducible tournaments

For the final evaluation every experiment is a configuration file, and every results file has a **manifest** next to it.

| In the manifest | Why |
|---|---|
| Commit of the code | which engine played |
| Hash of the configuration | which experiment it was |
| Hashes of the lexicon, the leaves and the models | which data it played with |
| Seeds, threads, flags | everything else that can change a game |

Resuming a run with a different configuration is refused. The simulation uses a fixed number of iterations and the endgame a depth limit instead of a time limit, so the same configuration replays the same games on any machine. A determinism test checks exactly that.

## Many comparisons at once

Run four experiments and call each significant at 5 %, and the chance that at least one is a false alarm is closer to 19 %. The analysis script applies the **Holm correction** across the experiments of a tournament: the p-values are adjusted so that the 5 % applies to the whole family.

`python -m ml.eval.tournament RESULTS...` turns any paired results file into win percentage and spread with paired intervals, Holm-corrected p-values, tables in Markdown and CSV, and a chart.

## Log-loss, accuracy and calibration

Games are not the only measure. The models are also scored directly:

- **Log-loss** (negative log-likelihood): minus the logarithm of the probability the model gave to what really happened, averaged. It rewards being right and punishes being confidently wrong. Lower is better.
- **Top-1 accuracy**: how often the most likely option was the true one.
- **Calibration (ECE)**: when the model says 30 %, does it happen 30 % of the time? The expected calibration error is the average gap.

## Tests and continuous integration

Every pull request runs, on GitHub Actions:

| Check | What it does |
|---|---|
| `g++` and `clang++` | builds the engine with warnings as errors and runs the C++ tests (more than 140) |
| `clang-format` | the C++ formatting |
| `ruff` | the Python lint and formatting |
| `pytest` | the Python tests |

The main branch is protected by these checks. A separate, manually started workflow runs long experiments in parallel shards, each with its own seed, and collects the results as one artifact.

The working rule is issue, branch, pull request: every experiment has an issue with its result written down. The numbers on this site are those issues.

## The lesson

> Report every result with paired games and a confidence interval. Two of the project's "signals", a bug in the endgame and an improvement from deeper playouts, were statistical noise or a fault in the measurement.
