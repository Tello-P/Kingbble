# kingbble

Engine and bot for English crossword word games, with Bayesian inference of the opponent's rack,
calibrated on human players, and risk-sensitive move selection. Bachelor's thesis (TFG).

## Status

**v1.0** (M7: reproducible tournaments and ablations). Source release; desktop binaries
come in v1.1. History: [CHANGELOG.md](CHANGELOG.md).

The tournament word lists (CSW24, NWL23) and their leave tables are licensed by their
owners and are not distributed: the thesis runs use them from a local copy. The public
build plays on ENABLE (public domain) with leaves trained here.

## What is inside

- **Engine (C++20)**: GADDAG/KWG move generation, static equity with KLV2 leaves, Monte
  Carlo simulation, exact endgame solver (Lazy SMP), exact and approximate pre-endgame.
- **Rack inference**: a particle/exact filter over the opponent's rack with a human move
  model (conditional logit by rating band, optional network or LightGBM) fitted on
  Woogles games, or the RangeFinder baseline; the sim bot draws its simulated opponent
  racks from it.
- **Risk**: RiskBot picks among simulated moves by mean-variance or win probability,
  with a learned risk policy.
- **Training**: data levels, gate between model versions, MLflow and an Auto loop
  (`python -m app.cli`, or the Streamlit interface).
- **Evaluation**: paired self-play tournaments with manifests and Holm-corrected
  ablations, matches against Quackle and Macondo, differential tests against Macondo.

Bots are named by their parts: `greedy`, `equity`, `sim`, `sim+inf:human` (or `:rf`,
`:none`), `risk`, `riskwin`, `human`, with `+apeg`, `+peg` and `+eg` for the
approximate pre-endgame, pre-endgame and endgame solvers, e.g. `sim+inf:human+eg`.

## Requirements

- CMake >= 3.20, GCC >= 12 or Clang >= 15 (C++20), git
- Python >= 3.11

## Build and test

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

pip install -e ".[dev,eval]"
ruff check . && pytest -q
```

## Lexicon

Official word lists and leaves are licensed and **are not committed to the repo**
(`lexica/` is in `.gitignore`); `scripts/issues/lib.sh` (`need_csw24`) says how to get
CSW24.

```bash
python scripts/fetch_lexicon.py --name NWL2023 --source /path/to/NWL2023.txt   # official
python scripts/fetch_lexicon.py --preset enable                                # free, for development
```

## Play

```bash
build/engine/kingbble_selfplay --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 \
  --human-params models/human_model.txt --a sim+inf:human+eg --b equity+eg \
  --games 20 --paired --per-game-seeds --threads 2 --results results.txt
python -m ml.eval.tournament results.txt
```

The thesis experiments, one command per table: [docs/experiments.md](docs/experiments.md).

## Structure

```
engine/      C++: include/, src/, apps/ (self-play, matches, evaluation), tests/
ml/          Python: data/, human_model/, rl/, eval/, tests/
app/         training menu (CLI and Streamlit) and the Auto loop
scripts/     lexicon, Quackle/Macondo helpers, per-issue run scripts (scripts/issues/)
configs/     tournament configs (configs/m7/)
docs/        experiments behind the thesis tables
```

Workflow: issue -> branch (`feat/*`, `fix/*`, `exp/*`, `docs/*`) -> PR -> green CI ->
merge into `main` -> tag per milestone.

## License and trademarks

Kingbble is free software under the GNU General Public License v3 ([LICENSE](LICENSE)).
Third-party parts and credits: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Kingbble is not affiliated with or endorsed by Hasbro or Mattel. Scrabble® is a trademark
of its owners.
