title: Questions
nav: Questions
blurb: Short answers about the demo, the app, the engine and the numbers.
lead: Short answers to what people ask first, each with the chapter that gives the long one.
---
## About playing

### Is it free?

Yes. Kingbble is free software under the GPL-3.0: you may use it, study it, change it and share it, and anything built from it must stay under the same licence. There are no accounts, no ads and nothing to pay.

### Why did it reject a word I know is valid?

The demo and the public app use ENABLE, a public-domain word list. It is close to the lists used in tournaments and not the same: some words valid there are missing here, and a few of ENABLE's are missing there. Tournament lists are licensed and cannot be included. See [the lexicon](lexicon).

### Why is Extreme locked in the browser?

It uses every core of the processor and longer thinking times, and a browser tab has one thread for the engine. It is in the [desktop app](download). The other four levels are the same in both.

### How strong is it?

At equal time, the full engine is level with Macondo's simulating bot within the uncertainty of 300 games: 47.0 % of games (41.7–52.3) with a spread of +4.8 points in its favour. That was measured on a tournament word list, with the desktop engine. The browser demo runs on one thread, so at the same seconds it computes less. See [references](references).

### Does the bot cheat? Does it see my tiles?

No. A bot receives what a player would see: the board, its own rack, the scores and the number of tiles in the bag. The Extreme level *estimates* your rack from your moves, which is what a strong human opponent does too, and it can be wrong. Only in the endgame, with the bag empty, does it know your rack exactly, as you know its rack, because it is everything that is not on the board. See [rack inference](inference).

### Can I play against another person?

On the same screen, yes: the Analysis mode is two players with both racks visible. There is no online play and none is planned.

### Can I play in Spanish?

The interface is in Spanish and English. The words are English: there is no Spanish word list that may be distributed freely, and the engine does not yet have the Spanish tile set. See the [roadmap](roadmap).

### Does it work on a phone?

Yes. The demo works in any current mobile browser. On a small processor, choose Medium or a short thinking time.

### Does it work offline?

The desktop app, always. The demo, once it has loaded, keeps working without a connection for as long as the tab is open, since nothing is asked of any server.

### Are my games saved?

No. The settings are kept on your device; games are not. Download a game as GCG if you want to keep it.

## About the engine

### What is different from other engines?

Three things. It was written from scratch, sharing no code with the reference engines and agreeing with them on every position compared. It estimates the opponent's rack over several turns with a model of how **people** play, where the classic method assumes a near-perfect opponent. And every claim comes with its number of games and its interval, including the ones that failed.

### Does the inference really help?

Against the classic method, RangeFinder, yes: 58.5 % of games (51.7–65.3) and a much better prediction of the tiles kept (5.74 against 12.50). Against not inferring at all, the gain in prediction against people is small (5.74 against 5.87), and its effect in games is one of the four final ablations, still to be run. See [results](results).

### Is it a neural network?

Mostly not. The move generator, the scoring, the simulation and the endgame are classic algorithms. Two parts are learned: the leave values, a table; and the move model, a small logit or network with a few dozen inputs. No part of the engine is a large model, and nothing needs a graphics card.

### Why not just simulate more?

It was tried. Sixteen times more iterations won no more games, and more candidates with few iterations lost. The limit is what the simulation assumes about the opponent, not how many times it is run. See [what did not work](lessons).

### Why C++?

The move generator runs millions of times in a simulation and the endgame searches millions of positions. The engine is C++20 with no dependencies; Python is used around it, for data, training and analysis.

### Why "Kingbble"?

It is the project's own name, with its own logo and colours. It is not affiliated with any maker of board games.

## About the numbers

### Why does every figure have an interval?

Because in this game the luck of the draw is large. Two identical bots playing 100 games will rarely end 50–50. A result without its interval cannot be told apart from luck. See [how it is measured](method).

### What does "paired" mean?

Each deal is played twice with the seats swapped. Whatever luck the deal held goes once to each bot and cancels, so far fewer games are needed to see a difference.

### Why are some results marked as not accepted?

A run against Quackle's strongest setting was stopped with Kingbble at 82 %. That is too good to believe without checking that the opponent was playing at full strength, so it is not counted. A number that flatters is checked harder, not less.

### Can I reproduce a result?

Yes: each table is one command in `docs/experiments.md`, with fixed seeds and a manifest of the exact code and files used. Those runs need the tournament word list, which you must obtain yourself. See [build from source](build).

## About the project

### Who made it?

It is a personal project by one person. See [about](about).

### Can I contribute?

Yes. The repository takes issues and pull requests; the workflow is issue, branch, pull request, green tests, merge. Good first targets are in the [roadmap](roadmap).

### Can I use the engine in my own program?

Under the terms of the GPL-3.0, yes: if you distribute a program that includes it, that program must be under the GPL-3.0 as well, with its source.

### I found a bug

Open an issue in the [repository](repo) with the position, as a GCG file or the seed of the game, and what you expected.
