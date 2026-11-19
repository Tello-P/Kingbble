title: Risk and time
nav: Risk and time
blurb: Should a bot that is behind gamble? RiskBot, and why it did not win more.
lead: A player who is behind should gamble and a player who is ahead should not. RiskBot tries to turn that intuition into a rule. It did not win more games, and the reasons are worth the chapter.
---
## Maximizing the average is not the same as winning

Simulation gives every candidate an average result. Choosing the best average is the right thing to do if you want the most points over many games. But a game is won by finishing ahead, by one point or by a hundred.

Picture being 60 points behind with few tiles left. A safe move that averages +5 and almost never swings loses the game for certain. A move that averages −2 but one time in five lands a seven-tile play is the only one that can win. Ahead by 60, the logic reverses: variance is the only thing that can hurt you.

That is the intuition behind the third hypothesis of the project.

## Two ways of choosing

RiskBot sits on top of the simulation. It takes the same candidates and the same playouts, and replaces the last step, "take the best average", with one of two rules.

**Mean-variance (`risk`).** Each candidate is scored by its average minus λ times its variance. A positive λ avoids risk; a negative one seeks it. λ changes with the state of the game: it depends on the score and grows as the bag empties, so early in the game the bot plays almost exactly like the plain simulation.

**Win probability (`riskwin`).** Each candidate is scored directly by an estimate of the chance of winning the game: the simulated outcomes are added to the current score, and the uncertainty of the rest of the game is modelled as a spread that shrinks as the tiles run out.

## What happened

RiskBot against the plain simulation bot, paired games on CSW24, 8 candidates and 50 iterations, for several values of λ.

:::bars min=30 max=70 ref=50 unit=% caption=Win percentage of RiskBot against the plain simulation bot for several risk settings. Paired games, 95 % intervals over the pairs. None differs from 50 %.
λ = 0.002 | 50.5 | 42.3 | 58.7 | 100 games
λ = 0.005 | 51.5 | 43.1 | 59.9 | 100 games
λ = 0.01 | 50.0 | 37.7 | 62.3 | 40 games
λ = 0.02 | 48.0 | 38.7 | 57.3 | 100 games
riskwin | 47.5 | 34.2 | 60.8 | 40 games
:::

| λ | Games | Win % | Spread per game |
|---|---|---|---|
| 0.002 | 100 | 50.5 (42.3–58.7) | −0.3 (−16.6 to +15.9) |
| 0.005 | 100 | 51.5 (43.1–59.9) | −7.2 (−23.3 to +8.8) |
| 0.01 | 40 | 50.0 (37.7–62.3) | −12.0 (−34.3 to +10.3) |
| 0.02 | 100 | 48.0 (38.7–57.3) | −9.2 (−24.9 to +6.4) |
| riskwin | 40 | 47.5 (34.2–60.8) | −14.1 (−38.7 to +10.6) |

No setting improves the win percentage. All are between 48 and 51.5 %, with intervals of ±8 or 9 points. The loss of spread is not significant either, though there is a weak tendency: the more risk is penalized, the more spread is given up.

## Why it does not win

The diagnosis matters more than the result.

1. **The variance being penalized is not the risk that matters.** Almost all of the variance in a playout comes from the opponent's rack and from the draw. It is nearly the same for every candidate. Penalizing it barely changes the order.
2. **The variance is badly estimated.** With 50 iterations it carries an error of about 20 %. With a high λ, that noise decides the move.
3. **The schedule barely acts.** The factor that scales λ with the bag leaves it close to zero for the first half of the game.
4. **The model of "the rest of the game" is not calibrated.** The spread assumed for the remaining turns was set by eye, and with it `riskwin` orders candidates almost exactly as the average does.
5. **The effect, if any, is small.** A realistic gain is 1 to 3 points of win percentage. Detecting +2 points would take about 5,000 games per setting. The full evaluation as planned would only have detected +5.

So the full run of nearly seven hours was not launched: the expected outcome was "no gain detected", at a high price.

## What would make it work

In order of cost and likely benefit:

- **Measure first**: in what share of turns RiskBot chooses a different move from the plain simulation, and how much average it gives up when it does.
- **A λ from theory**, derived from the lead and from the spread of the rest of the game, with no hand-tuned parameter.
- **Calibrate the spread of the rest of the game** on the human turns or on self-play.
- **Relative variance**: the variance of *candidate minus best* over the same deals, which removes the noise common to all.
- **A window**: apply risk only with the bag between 7 and 25 tiles and the game undecided.
- **A learned policy**: let the bot learn the correction by reinforcement. This is implemented as a learned risk policy in the training code and stays parked until there is a sign that risk helps at all.

## Time, the other half of the milestone

The same milestone turned the simulation from "a fixed number of iterations" into something that manages time, as described in [simulation](simulation): a budget in seconds per move, a stop as soon as one candidate is clearly ahead, and the guarantee that an unreached time limit changes nothing. With a fixed number of iterations the result is deterministic, which the tests check: a limit of 1,000 seconds that is never reached gives exactly the same averages, iterations and move as no limit.

Each layer has its own budget per move:

| Layer | Option |
|---|---|
| Simulation | `--sim-time` |
| Exact pre-endgame | `--peg-time` |
| Approximate pre-endgame | `--apeg-time` |
| Endgame | `--endgame-time` |

What is **not** implemented is a reserve for a whole-game clock, such as 25 minutes per player, where time saved on easy moves is spent on hard ones. The apps of the project always play with time per move. The interface does have a game clock for you; the engine simply does not plan around it.
