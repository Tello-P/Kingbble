# Per-issue run scripts

Run from the repo root. Logs go to `build/issue_runs/issue_<N>.log`; every script takes `--dry-run`.
`run_all.sh [--dry-run] [--players LIST] [ISSUE...]` runs them in the order below with an overall %.

| Issue | Checks | Command | Est. time |
|---|---|---|---|
| #31 (#29) | approx. pre-endgame self-play, win % >= 50, spread >= 0 | `issue_31_apeg_selfplay.sh` | ~20 min |
| #33 (#25) | 140 endgames (<= 8 tiles) vs Macondo, 0 mismatches | `issue_33_endgame_macondo_25.sh` | ~45 min |
| #32 (#23, #26) | 50 endgames (<= 14 tiles) vs Macondo, 0 mismatches | `issue_32_endgame_macondo_full.sh` | ~1 h+ |
| #30 (#20, #27, #28, #32 part 2) | pre-endgames with pass, bag 1 and 2, 0 mismatches | `issue_30_preendgame_macondo.sh` | ~6 h |
| #34 (#21, #24) | Quackle match 2,000 games >= 45 %; Lazy SMP 8 vs 1 thread | `issue_34_quackle_and_smp.sh [--match-only\|--smp-only]` | ~20 h |
| #38 | >= 100,000 human turns with rack | `issue_38_collect_turns.sh --players LIST` | ~3 h (network) |
