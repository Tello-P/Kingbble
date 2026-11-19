title: Equity and leaves
nav: Equity and leaves
blurb: A move is worth its points plus the tiles it keeps. Where those values come from.
lead: A move is worth the points it scores plus what the tiles it keeps will be worth later. This chapter is about that second number: what a leave is, where its value comes from and how Kingbble builds its own table from nothing.
---
## Points are half the story

Imagine two moves. One scores 30 and leaves you holding `UUVW`. The other scores 22 and leaves you `AEINRS`. Almost every strong player takes the 22: with those six tiles, a seven-tile play next turn is likely, and that is worth 50 points on top of the word.

The tiles kept after a move are its **leave**. The **static equity** of a move is

```
equity = points of the move + value of the leave
```

and the `equity` bot simply plays the move with the highest equity. It is "static" because nothing is simulated: both terms are looked up or computed on the spot. That makes it instant, and it is the first decision layer everything else builds on.

## What a leave table is

A leave table holds one number for every multiset of one to six tiles the bag allows. There are **914,624** of them. The number answers: *compared with an average draw, how many points better or worse off am I, over the rest of the game, for keeping exactly these tiles?*

Some typical shapes, whichever table you use:

- A blank is worth far more than any letter: around 20 to 25 points.
- An `S` is worth several points; a second `S` is worth less than the first.
- `Q` without a `U` is strongly negative.
- Duplicates hurt: `II` and `UU` are bad, `VV` is terrible.
- Balance matters: three vowels and three consonants beat six of either.
- Combinations are not sums. `ERS` is worth more than `E` + `R` + `S`, which is why the table has an entry for every combination and not just one per tile.

## Three table formats

| Format | What it is |
|---|---|
| **KLV** | binary: a small automaton that lists the leaves in order, then one 16-bit value per leave |
| **KLV2** | the same with 32-bit floating-point values; the format of Macondo and wolges |
| **CSV** | `LEAVE,value` lines; what Kingbble's own fitting tools write |

The engine reads all three. In memory a leave is packed into a 64-bit key (five bits per tile, sorted), and looking one up is a binary search. A leave that is not in the table is worth zero.

## The rest of the formula

Two adjustments make equity behave at the two ends of the game.

**Empty bag.** With no tiles left to draw, a leave is no longer a promise: it is a liability. Going out earns twice the value of the opponent's rack, and every tile kept costs twice its value plus a constant, as Macondo does. In practice the [endgame solver](endgame) replaces this estimate whenever it is switched on.

**Nearly empty bag.** With a few tiles left the table's values are too optimistic: there may not be time to use that blank. An optional blend moves the leave value from the table towards minus the face value of the tiles as the bag empties.

Exchanges go through the same formula: an exchange scores zero and its leave is whatever you keep, so the engine can compare "exchange five" with "play for 12" on equal terms.

## Where the values come from

The published tables for the tournament lists are licensed and are not distributed with Kingbble. So the engine learned to make its own, starting from nothing.

The method is the one used by wolges and Magpie, sometimes called the *full-rack* method:

1. **Play.** The `equity` bot plays hundreds of thousands of fast games against itself with the current table. On every turn that starts with a full rack and at least seven tiles in the bag, the game records the seven tiles and the equity of the move that was made.
2. **Average by rack.** Each distinct rack ends up with a mean: what holding those seven tiles was worth, on average.
3. **Derive the leaves.** The value of a leave is the mean value of all the full racks it could be completed to, weighted by the probability of drawing each completion, minus the value of a rack drawn from a full bag.
4. **Shrink.** A leave seen in few racks has a noisy estimate. The data is split into two independent halves; what the halves agree on is signal and the rest is noise, and each correction is shrunk towards zero accordingly (an empirical Bayes estimate).
5. **Repeat.** The corrected table is used to play the next generation, because better leaves change which moves are made and therefore which racks are worth what.

Racks are *dealt*, not chosen, so the sample carries no bias from the policy that played them.

The first generation starts from a table where every leave is worth zero, which is the greedy player. One generation is enough to recover the obvious, and the next ones refine it. The table that ships with the [web demo](play) took five generations of 600,000 games each, three million in all:

| Leave | Gen. 1 | Gen. 2 | Gen. 3 | Gen. 4 | Gen. 5 |
|---|---|---|---|---|---|
| `?` (a blank) | +21.0 | +26.6 | +28.6 | +29.3 | +29.5 |
| `S` | +5.7 | +7.6 | +8.2 | +8.5 | +8.6 |
| `Q` | −5.2 | −7.9 | −9.1 | −9.5 | −9.6 |

Each generation moves the values less than the one before. By the fifth the changes are a few tenths of a point, which is where the method has converged.

:::note The table in the demo and in the public release
It was fitted from scratch on ENABLE with this pipeline, with no published values anywhere in the process. It is Kingbble's own and is distributed with it. The desktop app can also keep training it on your machine: see [training](training).
:::

## How good is equity on its own

Good enough to be a serious opponent for most people, and blind in a specific way: it does not look at the board. It cannot tell that a move hands the opponent a triple-word square, or that the 40 points available now will still be there next turn. Those are the things [simulation](simulation) sees.

For reference, the [endgame chapter](endgame) measures `equity` plus an exact endgame against plain `equity`: the endgame alone is worth 4.8 points per game.
