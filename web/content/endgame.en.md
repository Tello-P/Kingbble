title: The exact endgame
nav: Endgame
blurb: With an empty bag there are no secrets. A search that plays the end of the game perfectly.
lead: When the bag is empty the game has no secrets left: the opponent holds exactly the tiles that have not appeared. From there Kingbble stops estimating and searches.
---
## A different game

For most of a game you reason about probabilities. The moment the last tile is drawn, that ends. You can work out the opponent's rack by subtraction, and from then on this is a game of perfect information, like chess, with a small number of moves left.

A good endgame is not the same as a high-scoring one. It is full of ideas that static equity cannot see:

- **Going out first.** Emptying your rack ends the game and earns twice the value of what the opponent still holds.
- **Sticking the opponent.** If they hold a `Q` with no place to play it, the right plan may be to play one tile at a time and collect, while they pass.
- **Blocking.** A play that scores little but takes away the opponent's only good square.
- **Setting up.** A play whose purpose is the play it makes possible next turn.

All of these fall out of searching the tree of moves and replies to the end.

## The search

The solver is a **negamax search with alpha-beta pruning**. In plain terms: it tries a move, then every reply to it, then every reply to those, down to the end of the game, and scores each final position by the difference in points. Working back up, each player is assumed to choose what is best for them. Alpha-beta skips branches that cannot change the answer: once a reply is found that already makes a move worse than an alternative, the other replies to that move need not be looked at.

What makes it practical:

| Technique | What it does |
|---|---|
| **Iterative deepening** | searches to depth 1, then 2, then 3… Each pass is cheap compared with the next and tells the next one which moves to try first. It also means there is always a complete answer to fall back on if time runs out. |
| **Move ordering** | trying the best move first is what makes alpha-beta prune. Moves are ordered by the previous iteration's best line and by score. |
| **Transposition table** | the same position can be reached by different orders of moves. Its result is stored the first time and reused. |
| **Lexicon pruning** | before searching, the [lexicon](lexicon) is cut down to the words that the tiles still in play could form. Move generation inside the search then walks a far smaller automaton. |
| **Lazy SMP** | on several threads, helpers search the same position into the shared table, half of them one ply deeper. The value is the same; the answer arrives sooner. The table is split into shards, each behind its own lock. |

The search stops at the first iteration that is **exact**, meaning no line was cut short by the depth limit, or at the time limit. The value it maximizes is the final **spread**.

Following Macondo's endgame convention, two consecutive scoreless turns end the game inside the search, and each player loses the value of their own rack.

## Is it right?

The check is the same as for the move generator: compare with the reference. If two solvers are both exact, the value of a position with perfect play must be identical.

Fifty real endgames, with up to 14 tiles between the two racks, were solved by both solvers with a limit of 600 seconds per position and side.

:::bars min=0 max=100 unit=% caption=Exact agreement with Macondo on endgames: 34 positions that both solvers finished within 600 s, zero differences in value.
Endgame value | 100 | | | 34 of 34 positions
:::

| | Endgames |
|---|---|
| Solved | 50 |
| Compared (both sides finish) | 34 |
| Differences in value | **0** |
| Skipped for time | 16: 5 not finished by Kingbble, 11 not finished by Macondo |

So in every endgame both could finish, the values are identical, and within the limit Kingbble finished more of them. Three positions were solved again from scratch on both sides as a further check and gave the same values.

## Is it worth it?

This question has a story. An early test showed the bot with the exact endgame winning only 47 % against the same bot without it. That would have meant a bug in the solver, and every stronger bot depends on it.

It was not a bug. The test had 200 unpaired games, and with that few the interval is ±7 points: "47 %" was compatible with "better". Replayed with [paired games](method), where each deal is played twice with the seats swapped so the luck of the draw cancels:

:::bars min=-1 max=7 ref=0 caption=Average spread per game gained over a bot with static equity only. 600 paired games per row, 95 % intervals.
Exact endgame, 1 s | 4.8 | 4.0 | 5.6
Exact endgame, 10 s | 5.1 | 4.3 | 6.0
1 s, falling back to equity if not exact | 2.7 | 2.0 | 3.3
:::

| Setting | Win % | Spread per game |
|---|---|---|
| 1 s per endgame | 51.4 (50.4–52.5) | +4.8 (+4.0 to +5.6) |
| 10 s per endgame | 51.8 (50.8–52.9) | +5.1 (+4.3 to +6.0) |
| 1 s, and equity's move if not exact | 51.0 (50.2–51.8) | +2.7 (+2.0 to +3.3) |

What this says:

- The exact endgame is **worth about 5 points per game**, significantly.
- It wins few extra games (about 1.5 points of win percentage) because the endgame only decides the result when the game is already close.
- With one second, **80 % of endgames are solved exactly**, at an average depth of about 4 plies. With ten seconds, 91 %. The extra time barely shows in the results.
- The solver plays something different from equity in about 36 % of endgames.
- When time runs out, using the move of the **last complete iteration** is better than giving up and playing equity's move (+4.8 against +2.7). So that is what the engine does.

## In the levels

The exact endgame is the difference between the Strong and Very strong [levels](levels) of the demo: `sim` against `sim+eg`. In the browser it runs on one thread with the thinking time you choose; in the desktop app it uses several.
