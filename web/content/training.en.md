title: Training
nav: Training
blurb: Train your own leaves from the app, and the training menu behind the models: levels, gate, MLflow, Auto loop.
lead: Two things in Kingbble are learned from games instead of written by hand: the leave values and the move model. Both can be retrained, and both are kept only if they pass a gate.
---
## Two things that are learned

| | Leave values | Move model |
|---|---|---|
| What it is | one number for each set of tiles kept | the chance that a player picks each move |
| Used by | every level from Medium up | rack inference |
| Learned from | the engine playing itself | human games, self-play, or a mix |
| Where it is trained | the desktop app, with one button | the training menu, from the command line |
| Explained in | [equity and leaves](equity) | [the human model](human-model) |

## Training leaves from the app

The desktop app has a **Train leaves** button in the settings, for the word list in use. It exists because a leave table belongs to its word list: a table made for one list is slightly wrong for another, and the tables published for tournament lists cannot be given away. With the button, any list you have gets its own.

| Training | How long | What it is for |
|---|---|---|
| **Fast** | minutes | to see it work; a rough table |
| **Normal** | one to two hours | a table good enough to play with |
| **Maximum** | overnight | the best the method gives on your machine |

What happens when you press it:

:::flow
Self-play | the engine plays fast games against itself with the current table
Credit | every full rack dealt is credited with the equity of the move made from it
Derive | each leave gets the average value of the racks it can be completed to
Shrink | values backed by few racks are pulled towards zero, using two independent halves of the data
Repeat | the new table plays the next generation
Gate | the new table plays the current one in paired games
:::

While it runs, the app shows the stage and the time left. **Stopping keeps the progress**: the next run continues from the last finished generation.

### The gate

A new table is not used because it is new. It plays paired games against the table in use, and the app reports the result with its interval:

- **Better than the current ones**: the interval is above 50 %. The new table is offered.
- **As good as the current ones**: the interval includes 50 %.
- **Worse than the current ones**: the interval is below 50 %. The current table stays.

You choose which table is in use, the included one or your own, and can go back at any time. Changing the leaves restarts the engine for that word list, so a game in progress is lost: the app warns you.

:::note The table that ships with the demo
The ENABLE table in the web demo and in the app was made exactly this way, starting from a table of zeros. No values were taken from any published table.
:::

## The training menu

The move model, and the experiments around it, are trained from the **training menu** in the repository: `python -m app.cli`, or a Streamlit page with the same options. It is a tool for the project, not part of the desktop app.

### Data levels

A level is an amount of data. Each is ten times the one before.

| Level | Name | Self-play games | Model | What it is for |
|---|---|---|---|---|
| 0 | smoke | 1,000 | logit | checks that the whole pipeline runs, in minutes |
| 1 | test | 10,000 | logit | a first usable model |
| 2 | base | 100,000 | logit and a network of 32 units | hours of generation |
| 3 | full | 1,000,000 | a network of 64 units | days of generation |

The large levels store a compact sample of their features and delete it after training, so that they fit on an ordinary disk.

### The commands

```
python -m app.cli                    # the interactive menu
python -m app.cli train --level 1    # generate what is missing, fit, save a new version
python -m app.cli evaluate           # gate the new version against the previous one
python -m app.cli improve            # go up a level if the learning curve still drops
python -m app.cli auto --budget-cpu-hours 20 --budget-gb 30
python -m app.cli status
python -m app.cli revert v003        # make an earlier version the active one
```

- **train** generates the self-play games the level still lacks, fits the model with its **learning curve** (the validation loss with growing fractions of the data) and saves a numbered version.
- **evaluate** is the gate: the log-loss of rack inference on held-out turns, and a paired match against the previous version. The verdict is *worse*, *not better* or *better*.
- **improve** looks at the learning curve. If more data was still lowering the loss, it goes up a level. If the curve is flat, more data would be wasted, and it says so.

### The Auto loop

`auto` repeats train and evaluate by itself within a budget of processor hours and disk.

1. Train at the current level.
2. Gate the new version against the best so far. Keep it unless it loses.
3. If the learning curve still drops, go up a level.
4. Stop when the budget is spent, when the last level is reached, when two levels in a row bring no improvement, or when a new version loses.

The best version is always the active model. The state is saved after every step, so the same command resumes after a shutdown. Before a level starts, the loop estimates the hours and gigabytes it needs and refuses if they do not fit in what is left.

### Safeguards

- Every heavy step runs with **limits on memory, cores and priority**, at the lowest priority, so the computer stays usable.
- A heavy run **refuses to start while another is active**, unless forced.
- Every run is **logged to MLflow**: parameters, the learning curve, the gate result and the files produced, so two versions can be compared months later.
- Any earlier version can be made active again with `revert`.

## What training cannot do

- It does not make the engine exact where it is exact already. The move generator, the scoring and the endgame are not learned.
- More data helps only while the learning curve drops. Level 0 passed its gate; whether level 3 is worth its days of machine time is an open question in the [roadmap](roadmap).
- A model fitted on other people's games carries their terms with it. That is why the public app trains leaves from self-play only and does not ship the human model.
