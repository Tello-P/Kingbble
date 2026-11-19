title: The human move model
nav: The human model
blurb: The probability that a person with this rack, at this rating, makes this move.
lead: The probability that a person holding this rack, at this rating and in this position, makes this move. What the model looks at, how it is fitted and how well it predicts players it has never seen.
---
## The question

Given a position, a rack and a player, the model answers: *of all the legal moves, how likely is each one to be chosen?*

An engine has a simple answer: the best move, always. A person does not. People miss seven-tile plays they do not know, prefer words they are sure of, hold on to an `S` too long, play safe when ahead and take risks when behind. Those tendencies are not noise. They are regular, they depend on the player's level, and they can be learned.

## A choice among options

The model is a **conditional logit**, the standard tool for choosing one item from a set. Each option gets a utility, a weighted sum of its features, and the probability of an option is its share of the total after exponentiation:

```
P(option i) = exp(u_i) / Σ exp(u_j)        u = w · features
```

Anything that is the same for every option of a turn cancels out of that fraction. So the model cannot learn "people score 35 on average", only *what distinguishes the chosen move from the others it competed with*. That is why equity enters as the gap to the best option of the turn, and why facts about the turn as a whole (the score, the stage of the game) enter mostly through interactions with features of the option.

For each turn the model looks at the top options by static equity, up to a cap `k`, plus the option actually played.

## What it looks at

The features come in groups, and each group answers a question about how people choose.

| Group | Question | Examples |
|---|---|---|
| **Base** | How good is the option by the engine's own measure? | gap to the best equity, points, leave value, rank, tiles played, whether it is a seven-tile play, an exchange or a pass |
| **A. The word** | Would the player know this word? | frequency in everyday English, times played in the training games, whether it is unknown on both counts, length, probability of drawing its letters, anagrams of the rack |
| **B. Missed bonuses** | Was a seven-tile play available and is this not it? | whether one existed, how many, how common the easiest one is, how much equity it gave up |
| **C. The leave** | What does the player keep? | size, vowels and consonants, duplicates, an `S`, a blank, `Q` without `U`, synergy between pairs, chance of a seven-tile play next turn, a count per letter |
| **D. Shape on the board** | Where does it go? | bonus squares used, cross words formed, hooks, tiles played through, distance to the centre, direction |
| **E. Defence** | What does it open? | triple-word squares newly in reach, lanes for a seven-tile play opened or closed |
| **F. Rank in the turn** | How does it stand out? | rank by points, equity standardized over the turn, gap between the best two options |
| **G. Context** | What is the game like? | score difference, tiles in the bag, turn number, empty bag, previous exchange, rating |
| **H. Interactions** | Do the above change with the stage, the lead or the rating? | equity gap × stage of the game, gap × rating, missed bonus × rating, word frequency × rating, points × lead |

The interactions are where the idea of Kingbble lives. "Gap × rating" lets the model learn that a 2,000-rated player almost never gives up 15 points of equity while a 1,400-rated one often does. "Word frequency × rating" lets it learn that obscure words are evidence of a strong player.

## By rating band

Players are grouped into bands by rating, with cuts at 1,400, 1,700 and 2,000. One set of weights is fitted on everyone, and then each band gets its own weights, **shrunk towards the common ones**. How strongly is chosen on validation data. A band with few turns stays close to the common model; a band with many is free to differ. A rating below every band, or an unknown one, uses the first.

## Two safety valves

A pure logit can assign a move a probability so small that one surprising move wipes out everything the tracker believed. Two mixture terms prevent that, each fitted per band:

- **epsilon**: with a small probability the player picks uniformly among the top options, whatever their utility.
- **residual**: with a small probability the player does something outside the options the model looks at.

They are a way of saying "people sometimes do things I cannot explain", with a number attached.

## The network, and the tree model

Two alternatives share the same features and the same output:

- **MLP.** A small neural network with one hidden layer on the standardized features. Its output, divided by a temperature, replaces the logit's utility. It can pick up non-linear effects the logit misses. The engine runs it directly from a text file: no machine-learning library at play time.
- **LightGBM.** A gradient-boosted tree ranker, available from the training menu as an alternative at the middle data level.

## How good is it

:::bars min=0 max=7 caption=Negative log-likelihood of the move actually played, on test players (lower is better). The model is the MLP fitted on real human games only.
Human model | 3.62 | | | fitted on real games, tested on unseen players
Uniform choice | 6.38 | | | every legal option equally likely
:::

A negative log-likelihood of 3.62 against 6.38 means the model puts, on average, about **16 times more probability** on the move that was really played than a uniform choice among the options would.

That is a measure of the model alone. What matters for the engine is whether it helps guess the *rack*, which is the next chapter.

## Why real games only

An earlier version padded the training set with self-play games from a "human-like" bot. The final model is fitted on real human games only: synthetic humans taught the model the bot's idea of a human. Self-play data still has its place, in the bot model and in the [training menu](training)'s data mix.

## The file

The fitted model is a plain text file: a version line, the kind of model, the feature names in order, the candidate cap, the mixture terms, and one line of weights per rating band. The feature names are a contract between the C++ engine that computes them and the Python code that fits them. A model file names its features, so an old model keeps working when new features are added.

:::note Not in the public release
The human model was fitted on games from Woogles, and whether a model trained on them may be redistributed is an open question put to the platform. Until that is settled the model file stays out of the public release, and the public Extreme level is built only from parts that need no human data. The results on this site are measured with the human model.
:::
