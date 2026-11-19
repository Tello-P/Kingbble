title: One move, from start to finish
nav: A move, step by step
blurb: What happens inside the engine between your move and its reply.
lead: What happens inside the engine between the moment you play and the moment it answers. A guided tour of every layer, with a link to the chapter that explains each one.
---
## The short version

When it is Kingbble's turn it holds seven tiles, sees the board and knows which tiles have not appeared yet. From there it does five things, each built on the one before.

:::flow
Generate | every legal move: about 500 per turn
Rank | by points plus the value of the tiles kept
Simulate | the best 10 or so against possible replies
Solve | exactly, when the bag is empty or nearly empty
Play | and tell the rack tracker what just happened
:::

How many of these steps run depends on the [level](levels): the easy level stops after the first, the medium level after the second, and so on.

## 1. Generate

The [lexicon](lexicon) is not a list of words. It is an automaton, a GADDAG, in which every word is stored once for each of its letters, so that a word can be built outwards from any tile already on the board. The [move generator](movegen) walks that automaton from every square next to a tile and places only letters that are on the rack and form valid words in the perpendicular direction.

The result is the complete list of legal placements, each with its [score](scoring). To that list the engine adds the exchanges it could make and the pass.

## 2. Rank

A move's score says what it earns now. Its [equity](equity) adds what the tiles it keeps are worth later: the *leave*. Keeping `ERS` is worth several points because those tiles combine well next turn; keeping `VVU` costs points. The leave values come from a table with one number for each of the 914,624 possible leaves of one to six tiles.

Ranking by equity is already a decent player. It is also instant: the medium level is exactly this.

## 3. Simulate

Equity cannot see that a move opens a triple-word square for the opponent, or that it blocks the lane you needed. [Simulation](simulation) can. For each of the best candidates the engine repeats the same small experiment many times: deal the opponent a rack, play the candidate, let the opponent make their best reply, make its own best follow-up, and write down the difference in points plus the value of the leaves left. The candidate with the best average is the choice.

Two details make this affordable. The simulation runs in batches and, after each one, drops the candidates that are clearly behind. And it works to a time budget: five seconds means five seconds.

This is the step where knowing something about the opponent's rack matters. With [inference](inference) switched on, most of the racks dealt to the opponent come from the tracker's posterior and not from a uniform draw.

## 4. Solve

With an empty bag there is nothing left to guess: the opponent holds exactly the tiles that have not appeared. The [endgame solver](endgame) searches the tree of moves and replies to the end and plays the line that maximizes the final difference. With one or two tiles in the bag the [pre-endgame solver](preendgame) does the same for every order in which those tiles could be drawn.

## 5. Play, and update what is known

The move goes on the board. If the bot uses inference, its tracker also observes every move *you* make: before your move it holds a cloud of racks you might have; after it, the racks that explain your move weigh more. The tiles you played leave those racks, new ones are drawn, and the cloud is ready for the next turn.

## Where each part lives

```
engine/      C++20: include/, src/, apps/ (self-play, matches, evaluation, bridge), tests/
ml/          Python: data/, human_model/, rl/, eval/, tests/
app/         training menu (command line and Streamlit) and the Auto loop
ui/          the interface: static page and local server
desktop/     the desktop wrapper
web/         this website and the WebAssembly build of the engine
scripts/     lexicon tools, helpers for Quackle and Macondo, one run script per experiment
configs/     tournament configurations
docs/        the experiments behind the results tables
```

The engine and the Python layer talk through bindings and through result files. The interface talks to the engine through one small program, the bridge: one text command in, one JSON object out. That is why the same page works over a local server, inside the desktop app and, in the [demo](play), over the engine compiled to WebAssembly with no server at all.

## Names of the bots

Bots are named by their parts, and the names appear all over this manual.

| Name | What it does |
|---|---|
| `greedy` | the move with the highest score |
| `equity` | the move with the highest static equity |
| `sim` | Monte Carlo simulation over the best candidates by equity |
| `sim+inf:human` | simulation with opponent racks drawn from the filter on the human model |
| `sim+inf:rf` | the same with the RangeFinder baseline |
| `sim+inf:none` | the same with uniform racks: the plain `sim` |
| `risk`, `riskwin` | choice among simulated candidates by mean and variance, or by win probability |
| `+eg` | adds the exact endgame solver |
| `+peg` | adds the exact pre-endgame solver |
| `+apeg` | adds the approximate pre-endgame |

So `sim+inf:human+eg` is the full bot of the experiments: simulation, inference with the human model and exact endgames.
