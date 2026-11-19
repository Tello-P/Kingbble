title: Play in the browser
nav: Play in the browser
blurb: The real engine, compiled to WebAssembly, at four levels. Nothing to install.
lead: The demo is the real engine, the same C++ compiled to WebAssembly, running inside this browser tab. Nothing is installed, and nothing is sent to a server: the bot thinks on your device.
---
:::raw
<div class="launch">
  <a class="tilebtn" href="../demo/index.html?lang=en"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M5 3.2v9.6L12.6 8z"/></svg><span>Play the demo</span></a>
  <small>About 4 MB the first time (the engine, the word list and the leaves). After that it loads from the browser's cache.</small>
</div>
:::

## What the demo is

The same interface as the desktop app and the same engine underneath: the move generator, the scoring, static equity with leaves, Monte Carlo simulation and the exact endgame solver. When you make a move, the engine checks it against the word list; when it is the bot's turn, it thinks for the time you gave it and plays.

It has three modes:

- **You vs Kingbble.** A game against the engine at the level you choose.
- **Bot vs bot.** Two bots play each other while you watch, with pause, step and speed.
- **Analysis.** Two players on one screen with every rack visible and the engine's best moves listed.

:::shot game-en.webp | The demo is this interface, running in the browser.
:::

## The levels

| Level | What it does | In the demo |
|---|---|---|
| **Easy** | plays the move with the highest score | yes |
| **Medium** | static equity: score plus the value of the tiles kept | yes |
| **Strong** | Monte Carlo simulation | yes, up to 5 s per move |
| **Very strong** | simulation plus the exact endgame | yes, up to 5 s per move |
| **Extreme** | the above plus rack inference, on every core | **locked**: only in the desktop app |

Each one is explained in [levels](levels). Extreme appears in the list with a padlock. It needs threads, more memory and more time than a browser tab should take, so it lives in the [desktop app](download).

## What is different from the desktop app

| | Web demo | Desktop app |
|---|---|---|
| Levels | Easy to Very strong | all five |
| Thinking time per move | 0.5 to 5 seconds | up to 60 seconds |
| Processor | one thread | every core |
| Word list | ENABLE | ENABLE, and your own lists |
| Leaves | the included table | the included one, or train your own |
| Training | no | [leaves training](training) with a gate |
| Saved between visits | settings only | settings |
| Works offline | after the first load | always |

The engine is single-threaded in the browser, so at the same number of seconds it does fewer simulations than the desktop app does. The moves are still legal, the scores are still exact, and the endgame is still solved, not estimated.

## The word list

The demo plays with **ENABLE**, a public-domain list of 168,551 English words (those of up to 15 letters are used). Tournament lists are licensed and cannot be given away, so they are not included anywhere on this site. If a word you know is rejected, that is the reason. More in [the lexicon](lexicon).

The leaves, the values of the tiles you keep, were trained from scratch for ENABLE by the engine playing itself. No published table is used.

## How to play

- **Place tiles** by dragging them, or click a square and type. Click the same square again to change direction.
- **Enter** plays the move. **Esc** takes the tiles back.
- **Exchange** lets you pick tiles to swap, while the bag has seven or more.
- A **blank** asks which letter it stands for.
- The **score preview** shows what the move is worth before you play it, and the panel lists the best moves if you turn it on in the settings.
- When the game ends, **review** it move by move, play again from any position, or download it as GCG.

The full list of controls is in [the interface](interface).

## If it does not load

| Symptom | Cause | What to do |
|---|---|---|
| It stays on "Loading the engine" | a slow connection: the first load is about 4 MB | wait, or reload |
| A blank page | a very old browser without WebAssembly exceptions | use a current Chrome, Edge, Firefox or Safari |
| The bot takes long on a phone | one thread on a small processor | lower the thinking time, or pick Medium |
| A level is greyed out | it is Extreme | it is in the [desktop app](download) |

## Privacy

There is no account, no analytics and no server behind the demo. The engine runs in a Web Worker in your browser. Your settings are kept in the browser's local storage and nowhere else.
