#!/usr/bin/env bash
# Creates the private repo, labels, milestones M0–M8 and the M0 and M1 issues.
# Requires: authenticated gh (gh auth login). Usage: scripts/bootstrap_github.sh [owner/]kingbble
set -euo pipefail

REPO="${1:-kingbble}"

if ! gh repo view "$REPO" >/dev/null 2>&1; then
  gh repo create "$REPO" --private --source . --remote origin --push
else
  git remote get-url origin >/dev/null 2>&1 || git remote add origin "$(gh repo view "$REPO" --json url -q .url).git"
fi
REPO="$(gh repo view "$REPO" --json nameWithOwner -q .nameWithOwner)"

for l in "task:1d76db" "bug:d73a4a" "experiment:5319e7" "engine:0e8a16" "data:fbca04" "thesis:c5def5"; do
  gh label create "${l%%:*}" --color "${l##*:}" --repo "$REPO" --force
done

milestone() {
  gh api "repos/$REPO/milestones" -f title="$1" -f description="$2" >/dev/null 2>&1 || true
}
milestone "M0 Scoping"       "Repo, CI, project proposal, lexicon script. Done when: CI green and advisor approves scope (v0.0)"
milestone "M1 Engine"        "Move generator. Done when: 10,000 positions identical to Macondo (v0.1)"
milestone "M2 Simulation"    "Equity, 2-ply sim, endgame. Done when: ≥45 % vs Quackle over 2,000 games (v0.2)"
milestone "M3 Data"          "Woogles, Parquet + DuckDB. Done when: ≥100,000 human turns with rack (v0.3)"
milestone "M6a Minimal menu" "CLI levels 0–1 and basic Auto loop (v0.35)"
milestone "M4 Inference"     "Core of the thesis. Done when: beats baseline in log-loss and win% (v0.4)"
milestone "M5 RL risk"       "Done when: significant win% gain (v0.5)"
milestone "M6b Full menu"    "Streamlit, levels 2–3, automatic k (v0.6)"
milestone "M7 Evaluation"    "Reproducible tournaments and ablations (v1.0)"
milestone "M8 Thesis"        "Thesis submitted (v1.0-tfg)"

issue() {  # title, milestone, labels
  gh issue create --repo "$REPO" --title "$1" --milestone "$2" --label "$3" --body "See roadmap." >/dev/null
}
issue "Repository, .gitignore, license and main protection" "M0 Scoping" "task"
issue "CI: engine (GCC/Clang), clang-format, ruff and pytest" "M0 Scoping" "task"
issue "Lexicon script (NWL/CSW) without redistributing lists" "M0 Scoping" "task,data"
issue "2-page project proposal signed by the advisor"        "M0 Scoping" "task,thesis"
issue "Check Macondo and Quackle licenses"                    "M0 Scoping" "task"

issue "Board, tiles, rack and bag"                           "M1 Engine" "task,engine"
issue "KWG loading"                                          "M1 Engine" "task,engine"
issue "GADDAG construction"                                  "M1 Engine" "task,engine"
issue "Anchors and cross-checks as bitmasks"                  "M1 Engine" "task,engine"
issue "Anagram WordMap"                                      "M1 Engine" "task,engine"
issue "Move generator"                                       "M1 Engine" "task,engine"
issue "Scoring"                                              "M1 Engine" "task,engine"
issue "GCG parser"                                           "M1 Engine" "task,engine"
issue "Differential test against Macondo (10,000 positions)" "M1 Engine" "task,engine"
issue "Benchmark of moves/s"                                  "M1 Engine" "task,engine"

# main only via PR with green CI (private repos: requires GitHub Pro or an education account)
gh api -X PUT "repos/$REPO/branches/main/protection" --input - <<JSON || echo "warning: main protection not applied (GitHub plan)"
{"required_status_checks":{"strict":true,"contexts":["C++ engine (g++)","C++ engine (clang++)","clang-format","Python (ruff + pytest)"]},
 "enforce_admins":false,"required_pull_request_reviews":null,"restrictions":null}
JSON

echo "Done: https://github.com/$REPO"
