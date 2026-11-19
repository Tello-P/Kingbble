title: What did not work
nav: What did not work
blurb: Five ideas that gave nothing, one false positive, and the bugs that taught the most.
lead: A project that reports only its successes is hiding half of what it learned. This chapter collects the ideas that gave nothing, a result that turned out to be noise, and the bugs that changed how things are measured.
---
## More computation did not win

The first suspicion when an engine loses to a stronger one is that it thinks too little. So the simulation was given more: more iterations for each candidate, more candidates, or both, against the base of 8 candidates and 50 iterations.

:::bars min=30 max=65 ref=50 unit=% caption=Win percentage of the bot with more computation against the base 8×50. 60 games each on a bank of 30 positions with 10 to 25 tiles in the bag. 95 % intervals.
8 candidates × 200 iterations | 46.7 | 39.0 | 54.4 | 4× the iterations
8 candidates × 800 iterations | 46.7 | 39.0 | 54.4 | 16× the iterations
15 candidates × 50 iterations | 42.5 | 36.2 | 48.8 | spread −11.6
15 candidates × 200 iterations | 50.8 | 43.2 | 58.4 | both
:::

Nothing wins. Going from 200 to 800 iterations changes nothing, and **more candidates with few iterations loses**. That last one is the *winner's curse*: with more candidates in the race, one of them is more likely to get an average inflated by luck, and the bot picks it. With 200 iterations the effect goes away.

The reading: beyond a small amount, the limit of the simulation is not noise, it is bias. It plays two plies ahead against an opponent who answers with a static move. More samples of a biased estimate give a more precise biased estimate.

## Deeper simulation: a false positive

If the problem is bias, look further ahead. Three plies instead of two. On the same bank, with the same iterations, the result looked like the first real improvement of the project:

| Measurement | Games | Win % (95 % interval) |
|---|---|---|
| First 60 games | 60 | 56.7 (48.9–64.4) |
| Extended to 200 | 200 | 56.5 (51.7–61.3) |
| Only the 140 new ones | 140 | 56.4 (50.4–62.5) |
| Full games | 80 | 53.1 (45.2–61.0) |

Significant, replicated on new games, and cheap: 13 % more time. Three plies became the default.

Two days later it fell apart. The position bank had a fault: **the random numbers were tied to the bot, not to the seat**. In a pair of games, the same deal with seats swapped, two identical bots did not always split 1–1. That left noise that did not cancel between the games of a pair, and since the bank was the same in every test, the same noise repeated in each of them. The "replication" replicated the fault.

| Three plies against two | Games | Win % | Spread |
|---|---|---|---|
| Old bank | 200 | 56.5 (51.7–61.3) | +2.1 (−2.9 to +7.2) |
| **Corrected bank** | **200** | **51.0 (47.5–54.5)** | **−0.5 (−4.6 to +3.7)** |

With the clean measurement, three plies ties with two.

:::note What was kept from it
A **null test** that now runs automatically: two identical bots on the bank must score exactly 50 %. If a harness cannot pass a test with a known answer, none of its other answers count. See [how it is measured](method).
:::

At equal time the picture was the same from the start: what the deeper bot gains from seeing further, it loses from doing fewer iterations. Four plies was no better than three, since each extra ply is played with a static answer and adds as much error as it removes.

## Risk: the average was hard to beat

The idea of [RiskBot](risk): when behind, prefer moves with a wider spread of outcomes; when ahead, narrower. Every setting of the risk parameter was tried.

:::bars min=30 max=70 ref=50 unit=% caption=RiskBot against SimBot (8×50), paired games, 95 % intervals.
λ = 0.002 | 50.5 | 42.3 | 58.7 | 100 games
λ = 0.005 | 51.5 | 43.1 | 59.9 | 100 games
λ = 0.01 | 50.0 | 37.7 | 62.3 | 40 games
λ = 0.02 | 48.0 | 38.7 | 57.3 | 100 games
Win probability | 47.5 | 34.2 | 60.8 | 40 games
:::

Everything between 48 and 51.5 %. Why:

1. **The variance being penalised is not the risk that matters.** Almost all of the variance of a move comes from the opponent's rack and from the draw, and it is the same for every candidate.
2. **The variance is badly estimated.** With 50 iterations it has an error of about 20 %. With a high λ that noise decides the move.
3. **The schedule barely acts.** The factor that turns risk on as the bag empties leaves it near zero for the first half of the game.
4. **The effect being looked for is small**, 1 to 3 points of win percentage. Detecting +2 takes about 5,000 games per setting.

The third hypothesis of the project, that a learned risk policy improves on the average, is reported as **not shown**. The learning of the risk parameter by reinforcement is parked for that reason.

## The approximate pre-endgame

With three to six tiles in the bag the exact solver is out of reach, so an approximate one samples orderings of the bag and solves each. Its measured effect: **51.5 % (49.3–53.7) and +0.5 points of spread** in 400 paired games. Neutral. It stays in the engine as an option and is not on by default in any level.

## Bugs that mattered

| Bug | How it showed | What it changed |
|---|---|---|
| **A leak in feature group A** of the human model | the group that should help made the model worse | fixed; A now improves the loss by about 0.06, and features are checked for using only what the player could see |
| **19,131 human turns discarded** by the parser | fewer turns with a rack than games implied | recovered; the model was refitted on more than 230,000 turns |
| **"The endgame does not beat equity"** | 46.5–47 % against equity in 200 unpaired games | not a bug: 200 unpaired games have an interval of ±7 points. Paired, the solver gains +4.8 points per game. Since then every comparison is paired |
| **Random numbers tied to the bot** in the bank | the false positive above | per-seat seeds and an automatic null test |
| **Quackle on the wrong board** | implausible results | the helper gave it a board with no premium squares, then one of the wrong size; both fixed, and the result still unverified |
| **An unknown bot name closed the engine** | the interface froze | names are validated before they reach the engine |

## What the failures say together

- **In this game, luck is large and effects are small.** An honest experiment needs paired games, hundreds of them, and intervals. Sixty games can show almost anything.
- **A result is not replicated if the replication shares the instrument.** The three-ply result held on new games because the fault was in the bank, not in the games.
- **A suspected bug can be noise too.** The endgame solver looked broken for a week because of 200 unpaired games. The measurement was at fault, not the solver.
- **The distance to the reference is in the model, not in the hardware.** More iterations did not close it. A better idea of what the opponent holds did move the needle, and that is the part this project is about.
