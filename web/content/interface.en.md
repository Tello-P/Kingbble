title: The interface
nav: The interface
blurb: Modes, controls, assists, review and settings, one by one.
lead: The interface is one page that knows nothing about the game. It draws what the engine tells it and sends back what you do. This chapter goes through everything on the screen.
---
## The screen

:::shot game-en.webp | Board on the left, panel on the right, rack and actions below the board.
:::

- **The top bar**: new game, download the game, and settings.
- **The player bars**: each player's name, score and clock, with the one to move marked.
- **The board**: fifteen by fifteen, with the premium squares in colour and, if you want them, coordinates.
- **The rack**: your seven tiles, with the actions beside it: play, recall, shuffle, exchange, pass.
- **The panel**: tabs for the best moves, the move list, the unseen tiles and the scoring help.

On a phone the panel moves below the board and the rack stays within reach of the thumb.

:::shot mobile.webp | The same page on a phone.
:::

## Three modes

| Mode | What it is for |
|---|---|
| **You vs Kingbble** | a game against the engine at the level and thinking time you choose |
| **Bot vs bot** | watch two bots play, with pause, next move and a speed control |
| **Analysis** | two players on one screen, every rack visible, with the engine's suggestions for whoever is to move |

When you start a game you choose the opponent, its thinking time, who moves first (you, the bot, or at random), the word list, and optionally a **seed**. The same seed deals the same tiles, so a game can be replayed or shared.

## Placing tiles

There are three ways and you can leave all of them on or pick one in the settings.

- **Drag** a tile from the rack to a square. Tap a placed tile to take it back.
- **Click and type**: click a square to put the cursor there, then type the letters. Click the same square again, or press Space, to switch between across and down. The cursor jumps over the tiles already on the board.
- **Click a suggested move** in the panel to place it for you.

A **blank** asks which letter it stands for. Holding Shift while typing a letter uses a blank on purpose, even if you hold the real tile.

## Keyboard

| Key | What it does |
|---|---|
| Letters | place that tile at the cursor |
| Shift + letter | place a blank as that letter |
| Arrow keys | move the cursor on the board |
| Space | change direction, across or down |
| Backspace | take back the last tile placed |
| Enter | play the move |
| Esc | recall all the tiles; in review, go back to the game |
| Left and right arrows | with no cursor on the board, go through the moves of the game |

## Assists

Each one can be turned off in the settings, for a game without help.

- **Best moves panel.** The moves for your rack ranked by static equity, with their score and the tiles they keep. This is the Medium level thinking aloud, not the simulation.
- **Score preview.** While tiles are on the board, the points the move would make, and whether it is valid.
- **Unseen tiles.** The tiles that are neither on the board nor on your rack: the bag plus the other rack.
- **Coordinates.** Letters and numbers on the edges of the board, as used in move notation: `8H` is row 8, column H.

## How a move scored

Click a move in the list to see where its points came from: each word formed, each letter with its multiplier, the word multipliers and the bonus for using all seven tiles.

:::shot breakdown.webp | The breakdown of one move: every word, letter by letter.
:::

The **Scoring** tab explains the premium squares, the rules of scoring in six lines with an example for each, and the value of every tile. The rules behind it are in [scoring](scoring).

## Reviewing a game

During or after a game, go back through it move by move. The board, the racks and the scores show the position after each move, and the list marks where you are.

:::shot review.webp | Reviewing: the board as it was after an earlier move.
:::

- **First, previous, next, current** step through the game.
- **Play from here** rewinds the game to that position and continues from it. It is a way of trying the other move.
- **Undo** takes back your last move and the bot's answer.
- **Download game (GCG)** saves the game in the standard format read by analysis programs.

## Settings

:::shot settings.webp | The settings dialog.
:::

| Setting | Options |
|---|---|
| Language | English, Spanish |
| Appearance | automatic (follows the system), light, dark |
| Placing tiles | drag, click and type; drag only; click and type only |
| Assists | best moves panel, score preview, unseen tiles, coordinates |
| Sound | on or off |
| Game clock | off, or a time for each player; running out ends the game |
| Intro animation and its sound | on or off; calm, music box, breath, or none |
| Leaves (desktop app) | the included table or one trained here: see [training](training) |

Settings are kept on your device between visits. Changing the language changes the interface; the word list stays the one you chose.

## Accessibility

- Everything can be done from the keyboard: the board is a grid you move through with the arrows, the rack and the tabs follow the usual patterns, and focus is always visible.
- Screen readers hear each square as its coordinate, its premium and its tile, and each tile as its letter and points. Moves and turn changes are announced.
- Colours meet the contrast needed for text in both themes, and the premium squares carry labels as well as colour.
- With "reduce motion" set in the system, the animations and the intro are off.
- Touch targets are large enough for fingers, and the page works from a narrow phone to a wide monitor.

## Messages

The engine explains itself in words. If a move is refused, the interface says why: tiles not in one row or column, the first move not covering the centre, a gap, a move not connected to the rest, or a word that is not in the list. An exchange with fewer than seven tiles in the bag is refused with that reason.

## How it talks to the engine

The page sends one command a line and receives one line of JSON:

```
new 7 human sim+eg 0 0
check 7,7,C 7,8,A 7,9,R 7,10,B
{"ok":true,"score":16,"move":{"kind":"place","text":"8H CARB (16)",...}}
```

In the desktop app a small local service passes those lines to the native engine. In the web demo the same lines go to the engine compiled to WebAssembly, in a Web Worker. The page does not know the difference, which is why the demo and the app look the same. More in [build from source](build).
