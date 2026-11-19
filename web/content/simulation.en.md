title: Monte Carlo simulation
nav: Simulation
blurb: Imagine the opponent's rack many times, play each candidate through, keep the best average.
lead: Imagine the opponent's rack many times, play each candidate through, and keep the one with the best average. How SimBot works, how it spends its time, and what was tried to make it stronger.
---
## What equity cannot see

Static equity ranks a move by what it earns and what it keeps. It knows nothing about what the move does to the board: whether it opens a lane to a triple word, sets up a hook only you can use, or leaves a bonus square the opponent will take. To value that, you have to look at what happens next.

You cannot know what happens next, because you do not know the opponent's tiles. But you can **sample** it.

## One iteration

For a candidate move, one iteration of the simulation is:

1. **Deal.** Give the opponent a rack drawn from the tiles you cannot see, and shuffle the rest as the bag.
2. **Play the candidate.** Put the move on a copy of the board and draw replacement tiles.
3. **Opponent replies** with their best move by static equity.
4. **You follow up** with your best move by static equity.
5. **Score it.** The value of the iteration is the difference in points over those plies, plus the value of the leave left at the end.

That is a *two-ply* playout: the opponent's reply and your follow-up. Repeat it a few hundred times with different deals and the average tells you how the candidate does against the range of things the opponent might hold.

:::flow
Candidates | the top 10 by static equity
Deal | a rack for the opponent, a bag order
Playout | candidate, reply, follow-up
Average | per candidate, with its standard error
Prune | drop those clearly behind; repeat
:::

Every candidate is evaluated against **the same deals**. Whatever luck there is in a deal affects all candidates alike, so the comparison between them is much sharper than each average alone.

## The parameters

| Parameter | Default | Meaning |
|---|---|---|
| `candidates` | 10 | how many of the best moves by equity are simulated |
| `max_iterations` | 200 | deals per candidate, at most |
| `min_iterations` | 32 | deals before anything can be pruned |
| `batch` | 16 | deals between pruning checks |
| `prune_z` | 2.58 | a candidate is dropped when it trails the best by this many standard errors (99 %) |
| `plies` | 2 | moves played after the candidate in each playout |
| `time_limit_s` | 0 | seconds per move; 0 means no limit |
| `posterior_share` | 0.8 | with a rack tracker, the share of opponent racks drawn from it; the rest are uniform |

## Spending time where it matters

**Pruning by confidence.** After each batch, every candidate whose average trails the best by more than 2.58 combined standard errors is dropped. The remaining iterations go to the candidates still in contention. As soon as only one is left the simulation stops, even if there is time to spare: an obvious move takes a fraction of a second.

**A time budget.** With a limit of, say, five seconds, the simulation runs batch after batch until the time is spent, always after the minimum of 32 iterations. The clock is checked at every iteration, so a long batch over many candidates cannot overrun it. The price is that the result then depends on how fast the machine is; for reproducible experiments the published runs use a fixed number of iterations instead.

**Halving.** An alternative to pruning that spends the whole budget: at equal steps of the time limit the worse half of the candidates is discarded. It lets more candidates be weighed in a fixed time.

**Threads.** Iterations are independent, so they are split across threads, each with its own generator and its own copy of the position. In the desktop app the simulation uses the cores you give it. In the browser it runs on one.

## Simulating the opponent better

Two refinements change *who* the simulated opponent is.

**Their rack.** By default the opponent's rack is a uniform draw from the unseen tiles. With [inference](inference), 80 % of the racks come from the tracker's posterior: the racks that fit how the opponent has been playing. The other 20 % stay uniform, as insurance against a tracker that is confidently wrong.

**Their reply.** By default the simulated opponent plays the best move by equity. Optionally it picks among its best few replies with probabilities that favour the better ones, a softer opponent that does not always find the top play, which is closer to how a person replies.

## Near the end

Leave values describe an average future. With few tiles unseen there is no average future left, only this one. So with at most a set number of unseen tiles the playouts can run **to the end of the game** instead of stopping after two plies, as Macondo does. And when the bag is empty the simulation steps aside for the [endgame solver](endgame).

## What was tried, and what it showed

Simulation is where most of the "obvious" improvements were tested. Nearly all of them turned out neutral, and one looked like a win until the measurement was fixed.

:::bars min=30 max=70 ref=50 unit=% caption=Win percentage of each variant against the base simulation (8 candidates × 50 iterations) on a bank of 30 positions played from both sides. 95 % intervals; the dashed line is 50 %.
8 × 200 iterations | 46.7 | 39.0 | 54.4 | 4× the iterations
8 × 800 iterations | 46.7 | 39.0 | 54.4 | 16× the iterations
15 × 50 | 42.5 | 36.2 | 48.8 | more candidates, same iterations
15 × 200 | 50.8 | 43.2 | 58.4 | more of both
3 plies, corrected bank | 51.0 | 47.5 | 54.5 | 200 games
:::

- **More iterations do not win.** Going from 50 to 200 or 800 iterations per candidate changes nothing measurable. The noise of the average is not the limit.
- **More candidates with few iterations lose.** This is the *winner's curse*: among 15 noisy averages, one is inflated by luck, and that is the one that gets chosen. With more iterations per candidate the effect goes away.
- **Deeper playouts tie.** Three plies first measured 56.5 %, which looked like the first real improvement. It was an artefact of the test bench: see [what did not work](lessons). On the corrected bench it is 51.0 %, a tie, at more time per move.

The conclusion that shaped the rest of the project: on these positions the gap to the strongest engines is not a matter of computing more. It is the bias of the model inside the playouts, a two-ply horizon against an opponent who plays statically, and the precision of the endgames.

## How much simulation adds

The `sim` bot against `equity` is one of the four ablations of the final evaluation ([results](results)). For the levels of the [demo](play), simulation is the difference between Medium and Strong.
