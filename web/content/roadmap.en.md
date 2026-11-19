title: Roadmap
nav: Roadmap
blurb: The milestones behind the engine, what is left before version 1.0, and what comes after.
lead: Kingbble was built milestone by milestone, each closed with a measurement. This is what each one added, what is still open, and the order in which the open things are planned.
---
## Where it stands

**Version 1.0 candidate.** The engine code is complete. Version 1.0 is tagged when the final runs, the four ablations, are done. The public release of the desktop app follows, with a free word list and its own leaves.

## The milestones

| | Milestone | What it added | How it closed |
|---|---|---|---|
| M0–M1 | **An engine of its own** | the lexicon as an automaton, the move generator, scoring | 0 differences with Macondo |
| M2 | **Search** | static equity with leaves, Monte Carlo simulation, exact endgame, exact and approximate pre-endgame | 0 differences in 34 endgames; +4.8 points per game |
| M3 | **Data** | parser, downloader, a store of turns, pseudonymised players, a split by player | 8,738 human games, more than 230,000 turns with a rack |
| M4 | **Rack inference** | the human move model, the rack filter, the RangeFinder baseline | log-loss 5.74 against 12.50; 58.5 % of games |
| M5 | **Risk and time** | RiskBot, simulation by time with a confidence stop, the match harness | risk: no gain detected |
| M6 | **Training** | data levels, the gate, MLflow, the Auto loop, own leaves | level 0 passes its gate |
| M7 | **Reproducible tournaments** | bots with inference in self-play, manifests, per-game seeds, Holm-corrected ablations | code complete; runs pending |
| M8 | **Interface and release** | the interface, the desktop app, this site and the web demo | in progress |

Each links to its chapter: [movegen](movegen), [simulation](simulation), [endgame](endgame), [data](data), [inference](inference), [risk](risk), [training](training), [method](method), [the interface](interface).

## Before version 1.0

| Task | Size | Status |
|---|---|---|
| The four ablations, 400 paired games each | about 22 hours of machine time | pending |
| The match against Quackle's strongest setting, after verifying its level | about 25 to 30 hours | stopped at 73 games; not accepted |
| The results table for each command in `docs/experiments.md` | hours | pending the runs |
| Tag `v1.0` | minutes | last |

## Open problems in the engine

**The pre-endgame with two tiles in the bag is slow.** Of 24 test positions, 11 do not finish within 600 seconds. Each candidate move forces an endgame to be solved for every order in which the tiles can be drawn, and a pass multiplies the tree. The plan: profile it, reuse endgames repeated between orderings, improve the move ordering and the pruning, and bound the pass earlier.

**One unexplained difference.** In one position with two tiles in the bag, one move gets the same number of wins as in Macondo and 18 points less of total spread. It is small, it is one case, and it is not understood, so it stays on the list.

**Close games against Macondo.** At equal time the match is a tie in games and in points, with more close games lost than won (17 against 27). The work on the last turns of the game, where that is decided, continues: see [references](references).

**Simulation depth and the opponent in the playouts.** More iterations did not help and three plies tied with two. What has not been tried at scale is a simulated opponent that plays better than a static move.

## The public release

The results were measured on a licensed tournament list. A public app cannot include it, so the release is built on what can be given away.

| Step | Status |
|---|---|
| Free word list (ENABLE) | done |
| Own leaves for ENABLE, trained from scratch | done; more generations running |
| Choose the Extreme level by measurement on ENABLE, about 400 paired games per pair of candidates | pending |
| Package the app without requiring Python on the user's machine | pending |
| "About" screen with the licence, the source and the credits | pending |
| Brand clean-up and the non-affiliation notice | done on this site; pending in the repository |
| Final check: no licensed file in the package, checksums of the word list, notices included | pending |
| Windows and Linux builds on the [download page](download) | pending |
| This site on GitHub Pages | built; not published yet |

Three questions are open with third parties, and until they are answered the cautious choice is taken: under which licence the published leave tables are; whether a model fitted on a platform's games may be redistributed; and whether a second free word list can be included.

## After 1.0

- **More word lists**, each with leaves trained from scratch by the button in the app.
- **Spanish.** Two things are missing: the Spanish tile set, with its Ñ and its two-letter tiles, and a word list that may be distributed.
- **A stronger simulated opponent**, using the bot model learned from self-play.
- **Risk, again**, with a variance measured relative to the best candidate and a parameter derived from theory, now that there is a harness able to detect small effects.
- **Analysis of a finished game**: mark the moves where the simulation disagrees with what was played, and by how much.
- **Threads in the browser**, when the hosting allows the headers WebAssembly threads need.

## What is not planned

- **Online play between people.** Kingbble is an engine and its interface. There are no accounts and no servers.
- **A mobile app.** The web demo works on a phone.
- **Including licensed word lists.** Not without the owners' permission.
