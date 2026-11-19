title: The problem and the idea
nav: The idea
blurb: Why this game is hard for a machine, and the hypothesis behind Kingbble.
lead: Why a crossword board game is hard for a machine, how the strongest engines deal with it, and the hypothesis Kingbble was built to test.
---
## Why this game is hard for a machine

No person beats a good chess engine any more: it sees the whole board, and it calculates further than anyone. Here a strong player still wins a fair share of games against the best engines, because three things get in the way at once.

- **Hidden information.** You do not know the seven tiles on the other rack, nor the order of the tiles left in the bag. Two positions that look the same on the board can be completely different games.
- **Chance.** What you draw changes the result a great deal. Two games with identical decisions can end 80 points apart. That also makes it hard to *measure* whether a change to the engine is an improvement: see [how it is measured](method).
- **Many moves.** There are on the order of 500 legal moves per turn, and the points a move scores are only half of its worth. The other half is the tiles you keep, the *leave*: playing `QI` for 22 and keeping `AEINRS` is not the same as scoring 30 and keeping `UUVW`.

## How the strong engines play

Quackle and Macondo, the two reference engines, share one recipe:

1. A very fast **move generator** lists every legal move.
2. A **static evaluation** ranks them: points plus the value of the leave, read from a table.
3. A **Monte Carlo simulation** takes the best few candidates and, for each, imagines many futures: a rack for the opponent, their reply, your next move. The candidate with the best average wins.
4. Near the end, when the bag is empty, the game has no secrets left and an **exact solver** takes over.

In step 3 the opponent's rack has to come from somewhere. By default it is drawn at random from the tiles you cannot see. Quackle also has RangeFinder, which discards racks that do not fit what the opponent just played, on the assumption that the opponent plays like an engine.

## The hypothesis

> People do not play like an engine: they make systematic mistakes that depend on their level and on the score. If I learn how people actually choose moves, every move of the opponent becomes evidence about the tiles they kept.

In symbols, Kingbble learns from real games the probability of a move given the rack, the player's rating and the state of the game. Then, during a game, it applies Bayes' rule: the racks under which the move just played would have been likely gain weight, and the racks under which it would have been strange lose it. Simulating against those more realistic racks should give better decisions.

Kingbble set itself three hypotheses that can be measured:

| Hypothesis | How it is measured | Status |
|---|---|---|
| H1. A model learned from people predicts the real move better than assuming an engine | Log-loss on players the model never saw | Confirmed |
| H2. Using that model in a multi-turn Bayesian filter improves play | Calibration and win percentage | Confirmed against RangeFinder; a modest gain against no inference |
| H3. A risk policy learned by reinforcement improves on maximizing the average | Change in win percentage | Not shown: RiskBot does not win more; the learning part is parked |

The goal is not to beat Macondo. It is to **measure what information about the opponent is worth**, with a model calibrated on people. That is why so much of this manual is about measuring.

## What had to be built

Nothing was borrowed from another engine. Every part below has its own chapter.

:::flow
Lexicon | GADDAG and KWG: every valid word in an automaton
Generator | every legal move and its score
Equity | points plus the value of the leave
Simulation | Monte Carlo over the best candidates
Endgames | exact solvers with an empty or nearly empty bag
:::

:::flow
Data | 8,738 human games with the mover's rack
Human model | probability of each move, by rating
Rack filter | a posterior over the opponent's rack, turn by turn
Simulation | deals the opponent racks from that posterior
:::

The first row is the engine, in C++20, where everything that must be fast lives. The second is the part that reads the opponent: the data and the learning are in Python, and the filter runs inside the engine. Around both there is a third, less visible part: the test harness that decides whether any of it works.

## What is new and what is not

Not new: GADDAG move generation, static equity with leave tables, two-ply simulation and exact endgames are known techniques, and Kingbble implements them as the literature describes. Their value here is that they are verified against the reference engine, so the experiments on top of them can be trusted.

New: the opponent model fitted on human games and conditioned on rating, its use inside a particle filter that follows the opponent's rack over several turns, the protection against rare moves, and the evaluation of all of it with paired games.
