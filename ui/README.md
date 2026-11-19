# Kingbble interface

Play against the engine, watch two bots, or analyse a game with two players on one
screen, in the browser or as a desktop app. The interface only talks to the engine
through `kingbble_bridge` (one text command per line in, one JSON line out), so engine
changes do not touch it.

```
engine (C++)  <-- stdin/stdout -->  kingbble_bridge  <-- HTTP -->  ui/server.py  <-->  ui/static (page)
                                                                        ^
                                                    desktop/main.js (Electron window)
```

## Web

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target kingbble_bridge
python ui/server.py --human-params models/human_model.txt --threads 2
# open http://localhost:8765
```

Every word list in `lexica/` (`.txt` or `.kwg`) can be chosen when starting a game; its
leaves are the file with the same name ending in `.klv2`, `.klv` or `.csv`. Lexica without
leaves only offer the greedy bot. Without any lexicon the server falls back to the small
test list in `lexica/fixtures/`.

## Desktop

```bash
cd desktop
npm install
npm start -- --human-params ../models/human_model.txt --threads 2
```

The app starts `ui/server.py` on a free port and opens it in its own window.

## What is in the page

- Modes: you against Kingbble, bot against bot (pause, step, speed) and analysis (two
  players, all racks visible).
- Opponent from a menu (greedy, equity, sim, sim+eg, sim+inf:human+eg) with a slider
  for its thinking time.
- Tiles by dragging, by clicking a square and typing, or both (Settings). Blanks ask for
  their letter; Shift+letter types a blank on purpose.
- Assists that can be switched off: best moves by static equity (click one to place it),
  score preview, unseen tiles, coordinates.
- Extras: undo, move history with racks, GCG download, sound, game clock, intro animation,
  light and dark themes, English and Spanish.

## Bridge commands

See the header of `engine/apps/bridge.cpp`. Example:

```
$ build/engine/kingbble_bridge --lexicon lexica/fixtures/enable_small.txt --leaves lexica/fixtures/leaves_small.klv2
{"ok":true,"ready":true,"words":2576,"leaves":true}
new 7 human equity 0 0
{"ok":true,"state":{"players":["human","equity"],...}}
check 7,7,C 7,8,A 7,9,R 7,10,B
{"ok":true,"score":16,"move":{"kind":"place","text":"8H CARB (16)",...}}
```
