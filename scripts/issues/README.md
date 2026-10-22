# Per-issue run scripts

Run from the repo root. Logs go to `build/issue_runs/issue_<N>.log`; every script takes `--dry-run`.
`run_all.sh [--dry-run] [--quick] [--players LIST] [ISSUE...]` runs them in the order below with an overall %.
`--quick` runs a bounded smoke check of each (not the final validation). Every finished game or
position is kept in `build/issue_runs/results/`: stop a run any time (Ctrl-C or kill) and start
the same command again to go on; `--status` reports how far each run is, `--fresh` starts over. Every run uses at most
`KINGBBLE_MAX_THREADS` cores (default 3) and `KINGBBLE_MAX_MEM` memory (default 6G, no swap), at the lowest priority.

The strength runs (#31, #34) and the data run (#38) use CSW24. Its word list and leaves are
licensed and stay out of git, in `lexica/`; `lib.sh` (`need_csw24`) says how to get them. The
Macondo comparisons check correctness and keep the free ENABLE list.

| Issue | Checks | Command | Est. time |
|---|---|---|---|
| #31 (#29) | approx. pre-endgame self-play, win % >= 50, spread >= 0 | `issue_31_apeg_selfplay.sh` | ~20 min |
| #33 (#25) | 140 endgames (<= 8 tiles) vs Macondo, 0 mismatches | `issue_33_endgame_macondo_25.sh` | ~45 min |
| #32 (#23, #26) | 50 endgames (<= 14 tiles) vs Macondo, 0 mismatches | `issue_32_endgame_macondo_full.sh` | ~1 h+ |
| #30 (#20, #27, #28, #32 part 2) | pre-endgames with pass, bag 1 and 2, 0 mismatches | `issue_30_preendgame_macondo.sh` | ~6 h |
| #34 (#21, #24) | CSW24 Quackle match 2,000 games >= 45 %; Lazy SMP 8 vs 1 thread | `issue_34_quackle_and_smp.sh [--match-only\|--smp-only]` | ~20 h |
| #38 | >= 100,000 human turns with rack | `issue_38_collect_turns.sh --players LIST` | ~3 h (network) |
