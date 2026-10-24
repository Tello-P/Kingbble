#include "scrabble/win_prob.hpp"

#include <algorithm>
#include <cmath>

namespace scrabble {

namespace {

double WinChance(double margin, double rest_sd) {
  if (rest_sd <= 0) return margin > 0 ? 1.0 : margin < 0 ? 0.0 : 0.5;
  return 0.5 * std::erfc(-margin / (rest_sd * std::sqrt(2.0)));
}

}  // namespace

WinEstimate EstimateWin(const std::vector<double>& values, double lead, double rest_sd) {
  WinEstimate est;
  const std::size_t n = values.size();
  if (n == 0) {
    est.win = WinChance(lead, rest_sd);
    return est;
  }
  std::vector<double> p(n);
  double sum = 0;
  for (std::size_t i = 0; i < n; ++i) sum += p[i] = WinChance(lead + values[i], rest_sd);
  est.win = sum / n;
  double sq = 0;
  for (double x : p) sq += (x - est.win) * (x - est.win);
  est.std_error = n > 1 ? std::sqrt(sq / (n - 1) / n) : 0;
  return est;
}

double RestSpreadSd(int bag_size, const WinParams& params) {
  return params.spread_sd_per_tile * std::sqrt(static_cast<double>(std::max(0, bag_size)));
}

std::vector<WinEstimate> WinProbabilities(const Game& game, const std::vector<SimResult>& results,
                                          const WinParams& params) {
  const int me = game.to_move();
  const double lead = game.score(me) - game.score(1 - me);
  const double rest_sd = RestSpreadSd(game.bag_size(), params);
  std::vector<WinEstimate> out;
  out.reserve(results.size());
  for (const SimResult& r : results) out.push_back(EstimateWin(r.values, lead, rest_sd));
  return out;
}

}  // namespace scrabble
