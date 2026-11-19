title: Kingbble
nav: Home
headline: The engine that tries to guess your rack
lead: Kingbble is a crossword-game engine written from scratch in C++. It generates every legal move, simulates the future, solves endgames exactly, and reads each move you make as evidence of the tiles you kept.
read: Read the manual
toc: What is in this manual
---
:::stats
0 differences | with Macondo, the reference engine, on 20,000 positions of the move generator and 34 exact endgames
58.5 % | wins for inference with a human model against the classic RangeFinder method (95 % interval 51.7–65.3)
5.74 vs 12.50 | log-loss when predicting the tiles a person keeps: own model against RangeFinder
47 % | against Macondo's simulating bot at equal time per move: a statistical tie (41.7–52.3)
:::

## Why a word game, and not chess

Nobody beats a good chess engine any more: the machine sees the whole board and calculates further than any person. A crossword board game is different. You see your rack and never the other one, the draw decides a lot, and a strong player still takes a good share of games off the best engines. So the machine cannot simply out-calculate you here. It has to understand you.

The strongest engines simulate the future assuming the opponent holds random tiles, or that it plays like a machine. Kingbble starts from another idea: **people do not play like machines, and the way they play gives their tiles away**. If someone could have played a seven-tile bonus for 80 points and exchanges instead, they almost surely did not hold the letters.

To test that idea the whole engine had to exist first: a lexicon stored as an automaton, a move generator that agrees with the reference engine position by position, Monte Carlo simulation, exact solvers for the end of the game, a model of human play fitted on more than 230,000 real turns, and a way of measuring that survives the amount of luck in this game.

This site is the manual of all of it. Each chapter explains one part: what it does, how it is built, how it was checked and what the numbers say, including the experiments that did not work.

:::shot game-en.webp | The interface: board, rack, and the panel with the best moves by static equity.
:::

## Three ways in

- **Play it.** The [demo](play) runs the real engine in your browser, compiled to WebAssembly, at four levels. The fifth, Extreme, is in the desktop app.
- **Install it.** The [download page](download) has the Windows and Linux builds of the desktop app.
- **Understand it.** Start with [the idea](idea), or go straight to the part you care about in the contents below.

:::note Honest numbers
Every figure on this site comes from a real run of the project, with its number of games and its 95 % interval. Where a result is neutral or negative it is reported as such: see [what did not work](lessons).
:::
