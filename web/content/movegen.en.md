title: The move generator
nav: Move generation
blurb: Anchors, cross-checks and a walk through the automaton: every legal move, 460,000 per second.
lead: Anchors, cross-checks and a walk through the automaton. How the engine lists every legal move of a position, and how it was proved to list exactly the right ones.
---
## The job

Given a board and a rack, return every legal placement: every word that can be laid in a row or a column, touching what is already there, using only tiles from the rack, with every perpendicular word it forms being valid too. A typical middle-game position has about 500 of them.

Everything else in the engine calls this code constantly. One simulated move needs a generation for the opponent's reply and another for the follow-up, thousands of times per second. So it has to be both exactly right and fast. Kingbble follows the algorithm Steven Gordon published in 1994, over the [GADDAG](lexicon).

## Anchors

Every new word must connect to the tiles on the board (or, on the first move, cover the centre square). So the search does not start from every square. It starts from the **anchors**: the empty squares next to a tile.

:::board 5 4 | Tiles on the board and, marked with a ring, the anchors of this position: the only squares from which a move can start to be built.
..+++++..
.+KANES+.
..+E+++..
..+R+....
...+.....
:::

On an empty board there is a single anchor, the centre.

## Cross-checks

A tile placed in a row also becomes part of a column. If there are tiles directly above or below the square, the letter placed there must complete a valid vertical word.

For each empty square and each direction the generator precomputes a **cross-check**: a 26-bit mask with one bit per letter that would be valid there, together with the points of the perpendicular word that would be formed. Squares with nothing above or below allow all 26 letters.

With the masks in hand, checking whether a letter may go on a square while building a word is one bit test, not a dictionary lookup. After each move only the masks of the squares near the new tiles are recomputed.

## The walk

From each anchor the generator walks the automaton and the board at the same time:

1. It starts at the anchor and moves **left** (or up, for vertical moves). At each square it either uses the tile already on the board or places one from the rack, and takes the matching transition in the GADDAG. A letter from the rack is only tried if the cross-check of that square allows it.
2. At any point where the GADDAG has a separator transition, it may **turn around**: jump back to the square right of the anchor and continue to the right in the same way.
3. Whenever the automaton says "a word ends here" and the next square on the board is empty or off the edge, a legal move has been found.

Because the GADDAG stores every word once per letter, each word is found exactly at the anchor where it belongs, and the search never explores a prefix that cannot be completed.

## Blanks, duplicates and the first move

- **Blanks.** A blank can stand for any letter. During the walk, when the rack holds a blank the generator tries it as each of the letters the current state and the cross-check allow, and records the tile as a blank, which scores zero.
- **Duplicates.** A move with a single tile forms a "word" both across and down, and would otherwise be listed twice. It is reported once.
- **The opening.** On an empty board horizontal and vertical openings are mirror images. The engine lists only one direction by default, as Macondo does; the interface asks for both, so you can open downwards if you like.

## Exchanges and passes

Exchanging is legal while at least seven tiles remain in the bag. The engine can list every distinct subset of the rack as an exchange. A pass is always available.

## Speed

In greedy self-play the generator produces about **460,000 moves per second** on one core of an ordinary laptop, for about 500 legal moves per turn and an average of some 396 points per player and game.

## How it was checked

Two independent checks, both automated.

**Against brute force.** On a small lexicon a second, naive generator tries every combination of tiles on every line. Both produce exactly the same moves.

**Against Macondo.** This is the one that matters.

1. `kingbble_positions` takes positions from self-play games and writes every move Kingbble finds in each.
2. Each position is exported in CGP, the text format Macondo reads.
3. A small Go program asks Macondo for all of its moves in the same position.
4. A script compares move by move and score by score.

:::bars min=0 max=100 unit=% caption=Exact agreement with Macondo. The generator was compared on 10,000 positions with seed 1 and another 10,000 with seed 2.
Move generator | 100 | | | 20,000 of 20,000 positions
:::

**Zero differences in 20,000 positions.** The raw totals differ (5.44 million moves against 5.19 million) only because, on an empty board, Macondo lists the opening in a single direction; those are counted as the same move.

To repeat it:

```
python3 scripts/compare_macondo.py --count 10000 --seed 1
```
