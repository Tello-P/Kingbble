title: Glossary
nav: Glossary
blurb: Every term used in this manual, in one line each.
lead: The words of the game and of the engine, each in a line or two, with the chapter where it is explained.
---
## The game

| Term | Meaning |
|---|---|
| **Rack** | the tiles a player holds, up to seven |
| **Bag** | the tiles not yet drawn |
| **Blank** | a tile worth zero that stands for any letter |
| **Premium square** | a square that doubles or triples a letter or a word, once, for the tiles placed on it that turn |
| **Bingo** | a move that uses all seven tiles; +50 points |
| **Hook** | a letter added to the front or the back of a word on the board to form another |
| **Cross-word** | a word formed perpendicular to the main word by one of its new tiles |
| **Exchange** | giving up the turn to swap tiles with the bag; allowed with seven or more tiles in it |
| **Going out** | playing your last tile with the bag empty; you earn twice the value of the opponent's rack |
| **Spread** | your score minus the opponent's |
| **Unseen tiles** | those neither on the board nor on your rack: the bag plus the other rack |
| **Lexicon** | the list of valid words. See [the lexicon](lexicon) |

## Notation and formats

| Term | Meaning |
|---|---|
| **8H** | a square: row 8, column H. Row first means across; `H8` means down |
| **`?`** | a blank on a rack. On the board, a lowercase letter is a blank played as that letter |
| **CGP** | one line of text describing a whole position: board, racks, scores |
| **GCG** | the text format of a whole game, move by move |
| **KWG** | the binary format of a word graph, shared with the reference engines |
| **KLV, KLV2** | binary formats of a leave table |

## The engine

| Term | Meaning | Chapter |
|---|---|---|
| **DAWG** | a graph that stores a word list by sharing prefixes and suffixes | [lexicon](lexicon) |
| **GADDAG** | a graph that stores every word starting from each of its letters, so moves can be built outwards from a tile on the board | [lexicon](lexicon) |
| **Anchor** | an empty square next to a tile, where a move may attach | [movegen](movegen) |
| **Cross-check** | for a square, the set of letters that form a valid cross-word there | [movegen](movegen) |
| **Leave** | the tiles kept after a move | [equity](equity) |
| **Equity** | the points of a move plus the value of its leave | [equity](equity) |
| **Static** | computed from the position alone, without looking ahead | [equity](equity) |
| **Simulation, playout** | playing the game forward with random draws to value a move | [simulation](simulation) |
| **Ply** | one move by one player | [simulation](simulation) |
| **Iteration** | one playout of every candidate with the same random draws | [simulation](simulation) |
| **Endgame** | the game from the moment the bag is empty: perfect information | [endgame](endgame) |
| **Pre-endgame** | the game with a few tiles left in the bag | [pre-endgame](preendgame) |
| **Alpha-beta** | a search that skips branches which cannot change the result | [endgame](endgame) |
| **Transposition table** | a memory of positions already searched | [endgame](endgame) |
| **Lazy SMP** | several threads searching the same tree and sharing that memory | [endgame](endgame) |
| **Bridge** | the program through which the interface talks to the engine, a line in and a line of JSON out | [interface](interface) |

## Inference and learning

| Term | Meaning | Chapter |
|---|---|---|
| **Rack inference** | estimating the opponent's tiles from the moves they make | [inference](inference) |
| **Prior** | what is believed before seeing the move: any rack the unseen tiles allow | [inference](inference) |
| **Likelihood** | how probable the move made is, for each rack they might have held | [inference](inference) |
| **Posterior** | what is believed after: prior times likelihood | [inference](inference) |
| **Particle filter** | a set of weighted candidate racks, updated after each move | [inference](inference) |
| **RangeFinder** | Quackle's classic method: keep the racks for which the move made was nearly the best | [inference](inference) |
| **Human model** | a model of which move a person picks, by rating | [human model](human-model) |
| **Conditional logit** | a model that gives each option a score and turns the scores into probabilities | [human model](human-model) |
| **MLP** | a small neural network | [human model](human-model) |
| **Self-play** | the engine playing against itself to produce data | [training](training) |
| **Gate** | a new version must not lose to the current one before it replaces it | [training](training) |

## Measuring

| Term | Meaning | Chapter |
|---|---|---|
| **Paired games** | each deal played twice with seats swapped, so the luck of the draw cancels | [method](method) |
| **95 % interval** | the range that would contain the true value in 95 of 100 repetitions | [method](method) |
| **Significant** | the interval does not include "no difference" | [method](method) |
| **Log-loss, NLL** | how surprised a model is by what happened; lower is better | [method](method) |
| **Nat** | the unit of log-loss with natural logarithms | [method](method) |
| **Calibration** | when the model says 30 %, it happens 30 % of the time | [method](method) |
| **Ablation** | removing one part to measure what it contributes | [results](results) |
| **Holm correction** | an adjustment for testing several hypotheses at once | [method](method) |
| **Null test** | an experiment whose answer is known, run to check the instrument | [lessons](lessons) |
| **Seed** | the number that fixes the random draws, so a run can be repeated | [method](method) |

## Names of bots

| Name | What it is |
|---|---|
| `greedy` | the highest score |
| `equity` | static equity |
| `sim` | Monte Carlo simulation |
| `sim+inf:human` | simulation with rack inference by the human model; `:rf` for RangeFinder, `:none` for none |
| `risk`, `riskwin` | selection by mean and variance, or by win probability |
| `+eg` | with the exact endgame |
| `+peg` | with the exact pre-endgame |
| `+apeg` | with the approximate pre-endgame |

So `sim+inf:human+eg` is simulation, inference with the human model, and the exact endgame.
