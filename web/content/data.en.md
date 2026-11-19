title: The data: 8,738 human games
nav: The data
blurb: Where the human games come from, how they are stored, and how players are kept anonymous and apart.
lead: A model of how people play needs people playing. Where the games come from, what is kept from each turn, how players are made anonymous, and why the split between training and test goes by player.
---
## What a model of human play needs

To learn *which move a person chooses with a given rack*, each example needs three things: the position, the rack the player held, and the move they made. The first and the last are in any game record. The rack is the rare one, because in most records only the tiles that were played are visible.

Woogles, an open platform for playing online, records the full rack of the player to move on every turn. That is what makes this dataset possible.

## From a game record to a row

:::flow
Download | finished games as GCG, with their metadata
Parse | replay each game and check every score
Store | one row per turn, in Parquet
Split | train, validation and test, by player
Features | every legal option of every turn, described by numbers
:::

**Download.** A crawler starts from a few seed players, lists each one's recent games, saves every finished classic game it has not saved yet, and queues the human opponents it meets. Requests are spaced out, and running it again only fetches what is missing.

**Parse.** Each game is replayed on a board by the [GCG parser](scoring). A game whose scores do not add up is discarded, so the store only holds games the engine reproduces move by move.

**Store.** Turns are written as Parquet files, partitioned by lexicon, and queried with DuckDB. Each row has the position before the move, the mover's rack, the move, the score, the state of the bag, whether the mover is a program, and the mover's rating.

## The numbers

:::stats
8,738 | human games in the CSW24 lexicon
230,000+ | human turns with the mover's rack known, after cleaning
3 | splits: train, validation, test, with no player in two of them
:::

Bots play on the platform too. Their turns are stored and flagged: they are left out of the human model and used to fit a separate *bot model*, the rival that inference assumes when the opponent is a program.

## Anonymous players

The store holds no names. Each nickname is replaced by a **pseudonym**: a salted SHA-256 hash, shortened. The same player gets the same identifier in every build, so their turns can still be grouped, but the identifier cannot be turned back into a name. The salt is a secret kept outside the repository.

## Why the split goes by player

The usual way to hold out test data is to set aside a random tenth of the rows. Here that would be a mistake. A player's habits repeat across their games: the same favourite words, the same blind spots. A model tested on other turns *of the same people* it trained on looks better than it is.

So the split is by player. A hash of the pseudonym sends each player, with all their turns, to exactly one of train, validation or test: 80 %, 10 % and 10 %. The assignment never changes as more games are added. Every number about the human model in this manual is measured on **players the model never saw**.

## A bug worth telling: the leak in familiarity

One group of features describes how familiar a word is: among other things, how many times it was played in the training games. The first version counted over all training games, *including the turn being predicted*. In training, the chosen move always carried one extra count that the other options did not. The model learned "the chosen move has the highest counter", a rule that does not exist outside the training set.

The symptom was that removing the whole group *improved* validation by 0.058. The fix was to count on games disjoint from the ones being fitted. With the leak closed the group helps, by about 0.06.

It is a small example of why the validation set exists, and of a rule followed throughout the project: when a number looks odd, find out why before using it.

## Features on disk

For each turn the engine generates every legal option and describes each with a vector of numbers: the [features](human-model). With hundreds of options per turn and hundreds of thousands of turns, this is the bulky part: tens of gigabytes at the larger data levels. To keep it manageable, only the top options by equity are kept per turn (plus the one actually chosen), rows are stored as 16-bit floats, and the files are memory-mapped so that fitting never loads them whole.

## Two sources, one format

Human games are one source. The other is **self-play**: games the engine plays against itself, written in the same GCG format and stored the same way. Self-play feeds the bot model and the [leave values](equity), and can be mixed with human data in the [training menu](training). For self-play the split goes by game, since every game has the same two "players".
