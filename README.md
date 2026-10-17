# kingbble

English Scrabble bot (NWL / CSW) with Bayesian inference of the opponent's rack,
calibrated on human players, and risk management via RL. Bachelor's thesis (TFG).

## Status

Milestone **M0 Scoping** (v0.0): repo, CI, lexicon script, project proposal.
M1 kick-off: tiles, rack, bag and board with premium squares.

## Requirements

- CMake ≥ 3.20, GCC ≥ 12 or Clang ≥ 15 (C++20), git
- Python ≥ 3.11

## Build and test

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

pip install -e ".[dev]"
ruff check . && pytest -q
```

## Lexicon

Official word lists are licensed and **are not committed to the repo** (`lexica/` is in `.gitignore`).

```bash
python scripts/fetch_lexicon.py --name NWL2023 --source /path/to/NWL2023.txt   # official
python scripts/fetch_lexicon.py --preset enable                                # free, for development
```

## GitHub

```bash
gh auth login
scripts/bootstrap_github.sh kingbble   # private repo, milestones M0–M8, M0 and M1 issues
```

Workflow: issue → branch (`feat/*`, `fix/*`, `exp/*`, `docs/*`) → PR → green CI → squash into `main` → tag per milestone.

## Structure

```
engine/      C++: include/, src/, tests/, bench/, lexicon/, tables/, movegen/, sim/
bindings/    pybind11
ml/          Python: data/, human_model/, inference/, rl/, eval/, tests/
app/         CLI/Streamlit menu and Auto loop
scripts/     lexicon, games, GitHub bootstrap
configs/     Hydra
```
