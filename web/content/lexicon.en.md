title: The lexicon: GADDAG and KWG
nav: The lexicon
blurb: How 168,551 words become an automaton that can be read from any letter.
lead: How a word list becomes an automaton that can be entered at any letter and read in both directions, which is exactly what this game needs.
---
## Why a list is not enough

A dictionary answers one question: is this word valid? The game asks a harder one: *which words can I build around the tiles that are already there?* A tile on the board may end up as the first letter of the new word, the last, or any letter in between.

A plain trie (a tree with one branch per letter, read from the first letter on) only helps when the word starts at the tile. For everything else you would have to try every starting square to the left and see whether the letters happen to line up. The GADDAG removes that guessing.

## The trick: store every rotation

For each word, a GADDAG stores one path for each of its letters. The path starts at that letter, reads **backwards** to the beginning of the word, crosses a separator and then reads **forwards** to the end.

Take `CASA`. It is stored four times:

| Path | Read as |
|---|---|
| `C^ASA` | start on the C; nothing to its left; then A, S, A to the right |
| `AC^SA` | start on the first A; C to its left; then S, A to the right |
| `SAC^A` | start on the S; A, C to its left; then A to the right |
| `ASAC` | start on the last A; S, A, C to its left; the word ends there |

:::tiles CASA
:::

So from any tile on the board, or from any empty square where a tile must go, the generator can extend to the left one letter at a time and, whenever it likes, turn around and extend to the right. Every step is a single transition in the automaton. There is never a position to guess.

## Keeping it small

Stored naively, that is as many paths as there are letters in the whole list. Two things keep it manageable.

**Minimization.** Many paths end the same way: every word that ends in `-ING` shares those last states. The automaton is built minimized with Daciuk's incremental algorithm, which merges equal suffixes as the words are added in sorted order, so the full unminimized tree never exists in memory.

**A flat array.** In memory the automaton is one array: each node points to a contiguous range of edges, and each edge holds a letter and the index of the next node. No pointers scattered around the heap, so walking it stays inside the processor's cache.

With the ENABLE list that comes to **401,729 nodes and 836,835 edges for 168,551 words, built in about 1.5 seconds**.

:::note Node and edge
A *node* is a state: "these letters have been read so far". An *edge* is a transition with one letter. Fewer nodes means less memory and, more importantly here, more of the automaton in cache at the same time.
:::

## KWG: speaking the same format as the others

KWG is the binary lexicon format used by wolges and Macondo. It is an array of 32-bit integers, each one an edge: its letter, whether a word ends there, whether it is the last of its siblings, and the index of its children. One file holds two automata, a DAWG to check words and a GADDAG to generate moves.

Kingbble reads and writes KWG and converts it to its own GADDAG. That was checked both ways:

- The `ENABLE.kwg` produced by wolges, the reference tool, yields **the same 168,551 words** when read by Kingbble.
- Macondo loads the KWG written by Kingbble without complaint.

The tool is `kingbble_buildkwg`. A small fixture of 2,576 words (`lexica/fixtures/enable_small.kwg`, 40 KB) lets the tests run without any licensed list.

## Anagrams: the WordMap

A second, simpler index sits next to the automaton. Its key is the letters of a word in alphabetical order (`RETAINS` becomes `AEINRST`) and its value is the list of words with exactly those letters.

Given some tiles, blanks included, it returns the words that use all of them, or all the words that use at least two. It is used to value racks and exchanges and to find seven-tile plays quickly, and it feeds several features of the [human model](human-model), such as how many seven-letter words a rack can make. It agrees with brute force on 200 random racks.

## Pruning the lexicon for one position

Near the end of the game most of the dictionary is dead weight: a word that needs a `Z` cannot be played once both players are out of `Z` and none is on the board in reach. Before an [endgame](endgame) search the engine builds a pruned automaton with only the words that the tiles still in play could form. The search then walks a far smaller structure.

## Which word lists

| List | Words | Licence | Used for |
|---|---|---|---|
| ENABLE | 168,551 | public domain | development, tests, the web demo and the public release |
| CSW24 | — | licensed (Collins) | the experiments; never distributed |
| NWL23 | — | licensed (NASPA) | comparisons; never distributed |

Official lists and their leave tables are licensed and are not in the repository. `scripts/fetch_lexicon.py` builds a normalized list (upper case, A to Z only, no duplicates) from a file you already have, or downloads ENABLE.
