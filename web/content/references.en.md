title: Against Macondo and Quackle
nav: Macondo and Quackle
blurb: The two reference engines: used first as a ruler, then as opponents.
lead: Macondo and Quackle are the two reference engines of this game. Kingbble uses them twice: first as a ruler, to prove its own answers are right, and then as opponents.
---
## Two references

**Quackle** is the classic analysis program, in C++, used by tournament players for two decades. Its strongest setting, Championship Player, simulates; its weakest, Speedy Player, plays statically. It introduced RangeFinder, the baseline that Kingbble's [inference](inference) is measured against.

**Macondo** is the current reference, in Go, the engine behind the bots of the Woogles platform. It has the strongest simulation and the most complete endgame and pre-endgame solvers that are openly available, and it reads the same lexicon and leave formats Kingbble does.

Neither is bundled, linked or copied. Kingbble talks to them from outside: a small helper program for each, started as a separate process.

## As a ruler

If two programs implement the same rules and both claim to be exact, their answers must be identical. That turns a reference engine into the strongest possible test.

| What | How it is compared | Result |
|---|---|---|
| Lexicon | the KWG written by wolges read by Kingbble; the KWG written by Kingbble loaded by Macondo | the same 168,551 words |
| Move generator | every move and score in 20,000 self-play positions, exported as CGP | 0 differences |
| Endgame | the exact value of 34 positions both solvers finish | 0 differences |
| Pre-endgame, 1 tile | wins and spread of the best moves over all orderings | 0 differences in 28 moves |
| Pre-endgame, 2 tiles | the same | 1 difference in 24 moves, under investigation |

These comparisons are scripts in the repository (`compare_macondo.py`, `compare_endgame.py`, `compare_preendgame.py`) with a configurable time limit and a dynamic distribution of positions among workers, so a slow position does not hold the others up.

## As an opponent: Macondo

The match: `sim+peg+eg` against Macondo's simulating bot, on CSW24, at **equal time per move and equal threads**. Five seconds per move, two cores, 300 paired games.

:::bars min=38 max=62 ref=50 unit=% caption=Kingbble against Macondo's simulating bot at 5 seconds per move, 300 paired games, 95 % interval.
Kingbble against Macondo | 47.0 | 41.7 | 52.3 | spread +4.8 (−6.3 to +15.9)
:::

**47.0 % of games, with an interval of 41.7 to 52.3, and an average spread of +4.8 points in Kingbble's favour** (−6.3 to +15.9). A statistical tie: the interval includes 50 %, and the spread is on Kingbble's side.

The interesting detail is in the close games. Of the games decided by a small margin, Kingbble **won 17 and lost 27**. It scores as many points as Macondo over a game and then loses more of the tight finishes. That points at the last few turns: the [pre-endgame](preendgame), where the exact solver is still slow with two tiles in the bag and where one unexplained difference remains.

This is what the next line of work was about: time management in the simulation, spending the whole budget on more candidates, valuing the leaves of both players at the end of a playout, and a *position duel*, a tool that takes the positions where the two engines chose different moves and judges them, exactly when the bag is small.

:::note Why equal time is the fair comparison
Giving both engines the same number of iterations would compare implementations. Giving them the same seconds on the same cores compares what a person sitting in front of each would get.
:::

## As an opponent: Quackle

Against Quackle the picture is incomplete, and it is told as it is.

| Opponent | Games | Result | Status |
|---|---|---|---|
| Speedy Player (the weak setting) | 10 | 80 % ± 25 | a smoke test, not a result |
| Championship Player | 73 of 300 | 82 % at the point it was stopped | **not accepted yet** |

The Championship run was stopped at 73 games with Kingbble at 82 %. That is too good to take at face value against Quackle's strongest setting, so before it counts, the harness has to be checked for whether Quackle is really playing at Championship level. Two faults in that helper have already been found and fixed along the way: for a time it gave Quackle a board with no premium squares, and then one of the wrong size.

The exit criterion written for this match is modest, a win percentage of at least 45 against Championship Player. Until the level is verified, no number is claimed.

## What the comparisons say

- On everything that has a right answer, moves, scores, endgames, Kingbble gives the reference's answer.
- In play, at equal time, it is level with the reference's simulating bot within the uncertainty of 300 games.
- The remaining distance is in the closing turns, not in the middle game and not in raw computation.

All of this was achieved by a single engine written from scratch by one person, running its experiments on a personal computer at low priority. The comparison is not there to claim a ranking. It is there so that the results about inference rest on an engine known to be sound.
