title: Rules, scoring and the game loop
nav: Rules and scoring
blurb: Premium squares, cross words, the 50-point bonus and how a game ends.
lead: How a move is scored, how a game runs from the first draw to the last tile, and the text formats the engine reads and writes.
---
## The board and the tiles

The board has 15 by 15 squares. Sixty-one of them carry a bonus, shown in the interface with a label and a pattern as well as a colour, so the board can be read without relying on colour:

| Label | Bonus | Squares |
|---|---|---|
| `2L` | double letter | 24 |
| `3L` | triple letter | 12 |
| `×2` | double word (the centre is one) | 17 |
| `×3` | triple word | 8 |

The English bag has 100 tiles: 98 letters, each with a value from 1 (`E`, `A`, `I`…) to 10 (`Q`, `Z`), and two blanks worth zero that can stand for any letter.

## Scoring a move

A move's score is the sum of every word it forms: the main word along the line of play, plus one perpendicular word for each new tile that touches tiles above or below it.

For each word:

1. Add the values of its letters. A letter on a **letter** bonus counts double or triple, but only if the tile on that square was placed this turn.
2. Multiply the sum by every **word** bonus under a tile placed this turn. Two word bonuses multiply each other: two doubles make ×4, two triples ×9.
3. Blanks count zero, whatever letter they stand for, though a blank on a word bonus still multiplies the word.

Finally, a move that places all seven tiles of the rack earns **50 extra points**, the bonus players call a bingo.

:::note Worked examples from the tests
`QI` on a double word: (10 + 1) × 2 = **22**. `RETAINs` as an opening play with a blank for the S, all seven tiles: **64** with the 50-point bonus. A cross word `.I` formed by one new tile next to a tile already on the board: **11**.
:::

The interface shows this arithmetic for every move: the Points tab breaks each word down letter by letter, with the bonus under each tile.

:::shot breakdown.webp | The score of a move, broken down word by word and tile by tile.
:::

## A game, from the bag to the last tile

The engine follows the NASPA tournament rules.

- **Start.** The bag is shuffled with a seed, and each player draws seven tiles. The same seed always gives the same bag, which is what makes experiments repeatable.
- **A turn** is one of three things: place tiles, exchange some of the rack, or pass. After placing, the player draws back up to seven.
- **Exchanges** are only allowed while at least seven tiles remain in the bag.
- **The game ends** in one of two ways:
  - A player empties their rack with the bag empty. That player adds **twice** the value of the tiles left on the opponent's rack.
  - **Six consecutive turns score nothing** (passes, exchanges, or zero-point plays). Each player then subtracts the value of their own rack.

The difference between the two final scores is the **spread**. Most of this manual reports two numbers for every experiment: the percentage of games won and the average spread per game.

## Seeds, done properly

Each game of a batch needs its own seed. The first version used `seed + g` for game `g`, which meant that `--seed 1` and `--seed 2` shared 199 of their 200 games, and two "independent" runs kept returning almost the same result.

Now every seed goes through SplitMix64, a mixing function, so batches are independent. With `--per-game-seeds` a game depends only on the run's seed, the deal and the seat, so a run that is stopped and resumed replays exactly the same games.

## Formats the engine speaks

| Format | What it is | Used for |
|---|---|---|
| **GCG** | the plain-text game record of Quackle and Woogles: `>Ana: AEINRST 8D RETINAS +72 72` | reading the human games, saving your own from the interface |
| **CGP** | one line describing a position: board, racks, scores | sending positions to Macondo for comparison |
| **KWG** | binary lexicon | sharing word lists with wolges and Macondo |
| **KLV / KLV2 / CSV** | leave tables | reading published and own leave values |

The GCG parser reads players, plays, exchanges, passes, withdrawn plays (`--`), challenge bonuses, final rack points and time penalties. It replays the game on a board and checks that every score and every running total match the file, which is also a second check of the scoring code against thousands of real games.

## The first bot: greedy

`greedy` plays the move with the highest score. If no placement exists it exchanges the whole rack when the bag allows it, and passes otherwise. It is a weak player and a very useful one: it is the fastest way to exercise the generator, the scoring and the game loop on millions of positions, and it is the Easy [level](levels).
