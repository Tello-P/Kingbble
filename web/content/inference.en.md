title: Inferring the opponent's rack
nav: Rack inference
blurb: A cloud of possible racks, reweighted by every move the opponent makes. The core of Kingbble.
lead: A cloud of racks the opponent might hold, reweighted every time they move. This is the core of Kingbble: how the filter works, how it protects itself from surprises, and what it is worth.
---
## The idea in one example

The opponent could have played a seven-tile word for 80 points through an open `E`. They exchange three tiles instead.

Before that move, any seven of the unseen tiles was a possible rack. After it, the racks that contained that seven-tile play are very unlikely: almost nobody passes up 80 points on purpose. The racks with three awkward tiles and four good ones have become likely, and the four tiles they kept are probably good ones.

That is Bayes' rule, and Kingbble applies it after every move the opponent makes:

```
P(rack | move)  ∝  P(move | rack) × P(rack)
```

`P(rack)` is what was believed before the move. `P(move | rack)` comes from a move model: the [human model](human-model) when the opponent is a person. The product, normalized, is what is believed now.

## Three move models

The filter is the same whatever model sits behind it. Three are compared throughout this manual.

| Model | Probability of the move given the rack | Name in bot names |
|---|---|---|
| **None** | the same for every rack: nothing is learned from the move | `inf:none` |
| **RangeFinder** | 1 if the move is the best option of that rack by equity (or within a tolerance), 0 otherwise | `inf:rf` |
| **Human model** | the probability a person of that rating would choose it | `inf:human` |

RangeFinder is the classic method of Quackle and Macondo. It is the right model for an opponent who always plays the best equity move, and a harsh one for anybody else: a rack is either fully compatible with the move or thrown away.

When the opponent is a program, Kingbble uses a separate **bot model**, fitted on bot turns, in place of the human one. In `auto` mode it picks between the two by who is moving.

## Why a particle filter

With 60 tiles unseen there are millions of possible racks of seven. Weighing each one would mean generating and ranking the moves of every rack, every turn. Far too slow.

So the filter keeps a **sample**: by default 500 hypotheses, each one a complete rack with a weight. These are the *particles*. One turn of the opponent goes like this:

:::flow
Observe | the opponent's move and the board before it
Reweight | each particle by P(move | its rack)
Resample | if too few particles carry the weight
Advance | remove the tiles played, draw new ones from the bag
Resync | after my own move and draw, fix what no longer fits
:::

**Reweight.** For each particle the engine ranks the options of that rack from the opponent's point of view and asks the model for the probability of the move that was actually made. A particle whose rack could not have made the move at all gets zero.

**Resample.** After a few turns most of the weight sits on a few particles. When the *effective* number of particles drops below half, the cloud is redrawn in proportion to the weights: heavy particles are copied, light ones disappear.

**Advance.** The tiles the opponent played leave every particle. What stays is each particle's guess at the **leave**, and that is the valuable part: new tiles are drawn at random, but the kept ones carry over to the next turn.

**Resync.** When I move and draw, I see new tiles, and some particles now contain tiles that cannot be on the opponent's rack. Those are *rejuvenated*: their tiles that still fit are kept and the rest are redrawn.

## Exact when it can be

Late in the game the number of possible racks collapses. When there are at most 3,000 of them, the filter stops sampling, **enumerates every rack** and updates each one exactly. At the switch, each rack starts with a share of the prior mixed in, so that a rack the sample happened to miss is not ruled out.

## Surprise

A filter that trusts its model completely is fragile. One strange move, a misclick, a deliberate fake or simply a word the model did not expect, and every particle gets a near-zero weight. What survives is whatever explains the strange move least badly, and the tracker ends up confidently wrong.

Kingbble measures how surprising each move is, in bits: how much likelier the move is under the *prior* (uniform racks) than under the tracked posterior. Above 4 bits, the posterior is **widened before the update**: half of it is replaced by the prior. One rare move then costs some sharpness instead of collapsing the whole belief.

This is what the stress test in [results](results) checks: an opponent who plays something outside their top ten moves 30 % of the time.

## Using it

The tracker hands the [simulation](simulation) a rack sampler. In each simulated iteration, 80 % of the time the opponent's rack is drawn from the posterior and 20 % of the time uniformly from the unseen tiles. Nothing else in the simulation changes.

## How well does it guess

The test: on held-out turns, how much probability did the tracker put on the tiles the player **really kept**? Measured as log-loss, in nats per turn; lower is better.

:::bars min=0 max=13 caption=Log-loss of the leave actually kept, on 2,000 held-out human turns from players the model never saw.
Kingbble, human model | 5.74 | | | 
No inference | 5.87 | | | uniform draw from the unseen tiles
RangeFinder | 12.50 | | | 
:::

:::bars min=0 max=13 caption=The same on 1,000 turns played by bots, with the bot model. Compare only within a panel: bots are more predictable, so everything scores lower here.
Kingbble, bot model | 4.46 | | | 
No inference | 5.40 | | | 
RangeFinder | 9.08 | | | 
:::

| On 2,000 human turns | Own model | No inference | RangeFinder |
|---|---|---|---|
| Log-loss | **5.737** | 5.865 | 12.497 |
| Top-1 accuracy | 21.4 % | 20.2 % | 20.6 % |
| Calibration error per tile (ECE) | 0.012 | 0.003 | 0.045 |

Read honestly:

- Against **not inferring at all**, the gain on human turns is real and small: 0.13 nats per turn. People are hard to predict.
- Against **RangeFinder**, the difference is large. RangeFinder is too sharp: it discards racks that were perfectly possible, and it pays heavily every time the player did not make the engine's top move.
- On **bots**, which are predictable, the gain over no inference is much larger: 0.94 nats.
- The per-tile probabilities are **well calibrated**: when the tracker says a tile is on the rack 30 % of the time, it is there about 30 % of the time. The error is about 1 %.

## Does it win games

Each side plays with simulation and tracks the other's rack with its own model.

:::bars min=40 max=70 ref=50 unit=% caption=Win percentage of inference with Kingbble's model against inference with RangeFinder. 200 games over two seeds, 95 % interval. The dashed line is 50 %.
Own model against RangeFinder | 58.5 | 51.7 | 65.3 | 117 of 200 games
:::

**117 of 200 games, 58.5 %, with a 95 % interval of 51.7 to 65.3.** Significantly better than even. It is the central result of the project.

By seed it was 54 % (average spread +2.9) and 63 % (+30.6), which also shows how much two batches of 100 games can differ.

And under stress, against an opponent who plays a random move outside their top ten 30 % of the time: Kingbble's model wins 89 % and RangeFinder 85 %, 100 games each. The difference is not significant (±9 points). The point of the test is the other one: the model does not break when the opponent stops making sense.

| Exit criterion of the milestone | Result | Met |
|---|---|---|
| Better than RangeFinder in log-loss | 5.74 against 12.50 | yes |
| Combined win % at least 50 | 58.5 % | yes |
| Against rare moves, no worse than the baseline | 89 % against 85 % | yes |

:::note The honest summary
The advantage in games comes mostly from **not being as wrong as RangeFinder**, and less from knowing much more than nothing. That is why the results report all three figures, none, RangeFinder and own model, each with its interval.
:::
