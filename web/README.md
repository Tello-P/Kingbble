# Kingbble website

The manual, the download page and the playable demo, as a static site for GitHub Pages.

```
web/
  build.py        the generator (standard library + Pillow)
  content/        one text file per chapter and language: SLUG.en.md, SLUG.es.md
  static/         site.css, site.js, img/ (screenshots), demo/ (worker, shim, engine)
  wasm/           the engine compiled to WebAssembly: web_bridge.cpp, build.sh
  data/           the demo's word list and leaves (not committed)
  dist/           the output (not committed)
```

## Build

```bash
pip install pillow
python web/build.py              # everything into web/dist/
python web/build.py --no-demo    # pages only, no engine or data needed
python -m http.server -d web/dist 8000
```

Open the site through a server, not as a file: the demo loads the engine with `fetch`.

## The demo's files

1. **The engine.** With [emsdk](https://emscripten.org) installed and activated:

   ```bash
   web/wasm/build.sh            # writes web/static/demo/engine.js and engine.wasm
   ```

2. **The word list**, `web/data/ENABLE.txt`: one word per line, upper case.

   ```bash
   python scripts/fetch_lexicon.py --preset enable
   cp lexica/ENABLE.txt web/data/ENABLE.txt
   ```

3. **The leaves**, `web/data/ENABLE-leaves.txt`: lines of `LEAVE,value` (one decimal),
   lines starting with `#` ignored, leaves worth zero left out. Made from a table
   trained with `kingbble_leavegen` and `kingbble_leavefit` (a CSV of `leave,delta`):

   ```bash
   python - <<'PY'
   import csv
   with open("cum.csv") as f, open("web/data/ENABLE-leaves.txt", "w") as out:
       out.write("# Kingbble leaves for ENABLE, trained from scratch by self-play\n")
       r = csv.reader(f); next(r)
       for leave, d in r:
           v = round(float(d), 1)
           if v: out.write(f"{leave},{v:.1f}\n")
   PY
   ```

   Only tables trained by this project go here. Do not copy a published leave table.

## What the demo allows

Set at the top of `static/demo/shim.js`: the bots offered (`greedy`, `equity`, `sim`,
`sim+eg`), the locked level shown with a padlock, and the longest thinking time (5 s).
The interface itself is copied unchanged from `ui/static/`.

## Download buttons

`content/download.en.md` and `download.es.md` hold the two buttons. They are placeholders:
set each `href`, and remove `aria-disabled="true"`, `tabindex="-1"` and the
"coming soon" label when the builds exist.

## Writing a chapter

A chapter is `content/SLUG.LANG.md`: a few `key: value` lines (`title`, `nav`, `blurb`,
`lead`), a line with `---`, then the text. Add the slug to `PARTS` in `build.py`.

- Links: `[text](slug)`, `[text](slug#anchor)`, `[text](demo)`, `[text](repo)`,
  `[text](repo:path)` or a URL. A link to an unknown slug stops the build.
- Tables: rows of `| a | b |`. Cells cannot contain `|`.
- Blocks, opened with `:::name` and closed with `:::`: `note Title`, `stats`, `flow`,
  `bars min= max= ref= unit= caption=...` (rows `label | value | low | high | note`),
  `board`, `tiles WORD`, `shot file.webp | caption`, `raw` (HTML as is).

## Publish on GitHub Pages

All links are relative, so `dist/` works at the root of `NAME.github.io` and in a
project folder alike. Copy the contents of `web/dist/` (including `.nojekyll`) to the
branch GitHub Pages serves. The chapter "Build from source" has a workflow that does
it on each push.
