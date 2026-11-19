title: The pre-endgame
nav: Pre-endgame
blurb: One or two tiles in the bag: solve an endgame for every way they could be drawn.
lead: The last few tiles in the bag are the hardest part of the game to play well. Kingbble has an exact solver for one or two tiles and an approximate one for three to six.
---
## Why the last tiles are special

With one tile in the bag, the opponent holds seven of the eight tiles you cannot see. You do not know which seven, but there are only eight possibilities. Each of them leads to a different endgame, and each of those endgames can be [solved exactly](endgame).

A move in this phase also decides who empties the bag, and therefore who gets to play the endgame with full knowledge first. Sometimes the best move is a small one that leaves a tile in the bag on purpose. Static equity and two-ply simulation both play this phase poorly, because the things that matter are exactly the ones they average away.

## The exact solver: one or two tiles

For each candidate move, the exact pre-endgame solver enumerates **every possible way the unseen tiles could be split** between the opponent's rack and the bag, in every order. For each one it plays the candidate, lets the draw happen, and solves the resulting endgame exactly. A candidate ends up with a count of wins, draws and losses over all the cases, and a total spread.

Moves are ranked **by wins first**. Among the moves tied for the most wins, the total spread decides. The goal here is to win the game, not to win it by more.

Details that keep it tractable:

- **Candidates.** Only the best moves by static equity are analysed in full, optionally with the pass.
- **Early cut-off.** A move is abandoned as soon as it has more losses than the best move so far could still end up with.
- **Nested solves.** A move that leaves tiles in the bag leads to another pre-endgame on the next turn. Those are solved to a limited depth.
- **Passing.** In nested positions a pass is only considered right after a scoreless turn, because that is when it can end the game.
- **Threads.** Moves and draws are distributed among threads, each with its own endgame solver.

## Checked against Macondo

Positions with one and two tiles in the bag, on ENABLE, 600 seconds per position and side. For each position the three best moves and the pass are compared: wins and total spread over all orderings of the bag.

:::bars min=0 max=100 unit=% caption=Exact agreement with Macondo on pre-endgames, by tiles in the bag.
1 tile in the bag | 100 | | | 28 of 28 moves, 7 positions
2 tiles in the bag | 95.8 | | | 23 of 24 moves, 6 positions
:::

| Bag | Positions compared | Moves compared | Differences | Skipped for time |
|---|---|---|---|---|
| 1 tile | 7 | 28 | **0** | 5 |
| 2 tiles | 6 | 24 | **1** | 6 |

With one tile in the bag the two solvers agree on everything compared.

With two tiles there is one difference, and it is reported here as it stands. For the move `M5 BLADE (24)` in one position both solvers give the same number of wins, zero, but the total spread over all orderings differs by 18 points: −8,732 for Kingbble and −8,714 for Macondo. The most likely explanation is that in one particular ordering one of the two finds a slightly better endgame line, or that a rule is applied differently, such as how a pass at the very end is treated. Wins being equal says the effect is small, but two exact solvers should not differ at all, so it is an open issue.

:::note The honest limit
With two tiles in the bag, 11 of 24 positions do not finish within 600 seconds. Each candidate needs an endgame for every order of the draw, and allowing the pass multiplies the tree. Making this faster, by reusing endgames that repeat between orderings and by cutting the pass earlier, is on the [roadmap](roadmap).
:::

## The approximate solver: three to six tiles

With more tiles in the bag there are too many cases to enumerate. The approximate pre-endgame, `apeg`, samples them instead:

1. Take the 10 best moves by static equity.
2. Deal the opponent's rack and the order of the bag at random from the unseen tiles, 100 times. Every move is tested against **the same deals**.
3. For each deal, make the move, let both sides play their best static-equity move until the bag is empty, and solve the endgame with a shallow search of 4 plies.
4. Rank the moves by the share of deals won, then by average final spread.

## What it is worth

The two layers together, `equity+apeg+peg+eg`, against a bot with only the exact endgame, `equity+eg`. Paired games, two seeds.

| | Win % | Spread per game |
|---|---|---|
| Seed 1 | 53.0 (49.4–56.6) | +1.8 (−1.9 to +5.4) |
| Seed 2 | 50.0 (47.6–52.4) | −0.8 (−4.1 to +2.5) |
| **Both, 400 games** | **51.5 (49.3–53.7)** | **+0.5 (−2.0 to +2.9)** |

In plain words: **no gain or loss is detected**. What can be said with confidence is that it does not make the bot worse, and that if it helps, it is by less than about 3 points per game.

That makes sense. These layers act only on the last two or three turns, and a bot that already solves the endgame exactly plays those turns well. Seeing an effect this small would take about 2,000 paired games.

This is also where the match against Macondo is lost by a little: Kingbble lost more close games than it won there (17 against 27), and close games are decided in these last turns. See [references](references).
