# Win % and mean spread of bot a with 95 % CIs over the finished pairs of a
# results file ("game N: S1 - S2, a in seat K"; games 2k-1 and 2k share a deal).
#   awk -v mode=NAME -v games=N -f paired_ci.awk RESULTS
match($0, /^game ([0-9]+): (-?[0-9]+) - (-?[0-9]+), a in seat ([12])/, m) {
  g = m[1]
  if (g > games) next
  a = m[4] == 1 ? m[2] : m[3]
  b = m[4] == 1 ? m[3] : m[2]
  score[g] = a > b ? 1 : a < b ? 0 : 0.5
  spread[g] = a - b
}

function se(sum, sq, n,   mean, v) {
  mean = sum / n
  v = (sq - n * mean * mean) / (n - 1) / n
  return v > 0 ? sqrt(v) : 0
}

END {
  for (g in score) {
    if (g % 2 == 0 || !((g + 1) in score)) continue
    p = (score[g] + score[g + 1]) / 2
    d = (spread[g] + spread[g + 1]) / 2
    n++; sum += p; sq += p * p; ssum += d; ssq += d * d
  }
  if (n < 2) { printf "[#54] %s vs sim: %d pairs, too few\n", mode, n; exit }
  mean = sum / n; w = 1.96 * se(sum, sq, n)
  smean = ssum / n; sw = 1.96 * se(ssum, ssq, n)
  lo = 100 * (mean - w); hi = 100 * (mean + w)
  printf "[#54] %s vs sim: %d games, a win %% %.1f (95%% CI %.1f-%.1f), a avg spread %+.1f (%+.1f..%+.1f), %s\n",
    mode, 2 * n, 100 * mean, lo, hi, smean, smean - sw, smean + sw,
    (lo > 50 ? "significant gain" : "no significant gain")
}
