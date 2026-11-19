title: Build from source
nav: Build from source
blurb: Compile the engine, run the interface, build the web demo and publish this site.
lead: Everything on this site is built from one repository: the engine, the interface, the desktop app, the web demo and these pages. This chapter is the list of commands for each.
---
## What you need

| For | Requirements |
|---|---|
| The engine | CMake 3.20 or later, GCC 12 or Clang 15 or later (C++20), git |
| The tools and the training | Python 3.11 or later |
| The desktop app | Node.js with npm |
| The web demo | Emscripten (emsdk) |
| This site | Python 3.11 with Pillow |

## The repository

```
engine/      C++: include/, src/, apps/ (self-play, matches, evaluation, bridge), tests/
ml/          Python: data/, human_model/, rl/, eval/, tests/
app/         the training menu (CLI and Streamlit) and the Auto loop
ui/          the interface: server.py and static/ (the page)
desktop/     the Electron window
web/         this site: content/, static/, wasm/, build.py
scripts/     lexicon, helpers for the reference engines, per-experiment run scripts
configs/     tournament configurations
docs/        the experiments behind the tables
```

## The engine

```
git clone https://github.com/Tello-P/Kingbble
cd Kingbble
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The tests need no licensed file: they use a small word list and leave table made for the purpose, in `lexica/fixtures/`.

For the Python side:

```
pip install -e ".[dev,eval]"
ruff check . && pytest -q
```

## A word list

Tournament lists and their leave tables are licensed and are not in the repository. The free list is one command:

```
python scripts/fetch_lexicon.py --preset enable
```

This downloads ENABLE into `lexica/`. A list you have the right to use is added with `--name` and `--source`. A leave table is found by name: a file beside the list with the same name ending in `.klv2`, `.klv` or `.csv`. A list without leaves offers the Easy level only.

## The interface in a browser

```
cmake --build build -j --target kingbble_bridge
python ui/server.py --threads 2
```

Then open `http://localhost:8765`. The server uses only Python's standard library. It starts one engine process per visitor, forwards the page's commands to it, and refuses any command it does not know before it reaches the engine.

## The desktop app

```
cd desktop
npm install
npm start -- --threads 4
```

The app starts the same local server on a free port and opens it in its own window. Everything after `--` is passed to the server.

## The web demo

The demo is the bridge compiled with Emscripten. With emsdk installed and activated:

```
web/wasm/build.sh
```

This writes `engine.js` and `engine.wasm` (about 570 KB) into `web/static/demo/`. The build is single-threaded and uses WebAssembly exceptions. In the page, three small files take the place of the server:

| File | What it does |
|---|---|
| `worker.js` | loads the engine in a Web Worker, fetches the word list and the leaves, and answers commands |
| `shim.js` | answers the page's `/api` calls from the worker, limits the levels and the thinking time, and shows Extreme as locked |
| `demo.css` | the few styles the demo adds |

The interface itself is copied unchanged from `ui/static/`. That is the reason for the design in [the interface](interface): one page, two engines behind it.

The demo needs two data files, which are not committed: `web/data/ENABLE.txt` and `web/data/ENABLE-leaves.txt`. The file `web/README.md` says how to produce each.

## This site

```
python web/build.py            # everything into web/dist/
python web/build.py --no-demo  # the pages only
python -m http.server -d web/dist 8000
```

The generator has no dependencies beyond Pillow, for the screenshots. Each chapter is two text files, `web/content/SLUG.en.md` and `SLUG.es.md`. A link to a chapter that does not exist stops the build.

The output is plain files with relative links only, so it works from any folder of any static host:

```
dist/
  index.html      sends you to en/ or es/ by browser language
  en/  es/        the chapters
  demo/           the playable demo
  assets/         styles, script, fonts, images
  404.html
  .nojekyll
```

## Publishing on GitHub Pages

Because every link is relative, the same `dist/` works as a user site (`https://NAME.github.io/`) and as a project site (`https://NAME.github.io/Kingbble/`) without changes.

**As a user site.** Copy the contents of `web/dist/` to the root of the repository `NAME.github.io` and push. In *Settings, Pages*, the source is the main branch, root folder.

**As a project site, by hand.** Copy `web/dist/` to a `gh-pages` branch of the repository and choose that branch in *Settings, Pages*.

**With an action.** A workflow that builds and publishes on each push to main:

```
name: site
on: { push: { branches: [main] } }
permissions: { contents: read, pages: write, id-token: write }
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: "3.12" }
      - run: pip install pillow
      - run: python web/build.py
      - uses: actions/upload-pages-artifact@v3
        with: { path: web/dist }
  deploy:
    needs: build
    runs-on: ubuntu-latest
    environment: github-pages
    steps:
      - uses: actions/deploy-pages@v4
```

For the action to build the demo, the compiled engine and the two data files have to be available to it: committed, or downloaded from a release in an earlier step.

:::note The .nojekyll file
GitHub Pages runs sites through Jekyll unless told not to, and Jekyll skips some files. The empty `.nojekyll` file at the root of `dist/` turns that off. Keep it.
:::

The site is about 14 MB on disk with the demo, most of it the word list and the leaves. GitHub Pages serves them compressed, so a visitor downloads about 4 MB for the demo and a few tens of kilobytes for a page.

## Running the experiments

Each results table is one command, listed in `docs/experiments.md`. They need the licensed tournament list, since that is what the results were measured on, and they run at the lowest priority with a set number of cores. Each has a `--quick` mode that checks the pipeline in minutes. See [results](results).
