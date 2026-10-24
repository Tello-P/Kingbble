#include "scrabble/risk_bot.hpp"

#include <algorithm>

namespace scrabble {

double RiskLambda(double lead, int bag_size, const RiskParams& params) {
  const double score = std::clamp(lead / params.lead_scale, -1.0, 1.0);
  const double phase = 1.0 - std::clamp(static_cast<double>(bag_size) / params.full_bag, 0.0, 1.0);
  return params.max_lambda * score * phase;
}

RiskBot::RiskBot(const Gaddag& gaddag, const Leaves& leaves, SimParams sim, RiskParams risk)
    : sim_(gaddag, leaves, sim), params_(risk) {}

Move RiskBot::Choose(const Game& game) {
  results_.clear();
  Move best = sim_.Choose(game);
  last_num_moves_ = sim_.last_num_moves();
  const std::vector<SimResult>& sims = sim_.last_results();
  const std::vector<WinEstimate> wins = WinProbabilities(game, sims, params_.win);
  const int me = game.to_move();
  const double lambda = RiskLambda(game.score(me) - game.score(1 - me), game.bag_size(), params_);

  for (std::size_t c = 0; c < sims.size(); ++c) {
    const SimResult& s = sims[c];
    const std::size_t k = s.values.size();
    double sq = 0;
    for (double v : s.values) sq += (v - s.mean) * (v - s.mean);
    RiskResult r{s, k > 1 ? sq / (k - 1) : 0, wins[c], 0};
    r.utility = params_.mode == RiskMode::kWinProb ? r.win.win : s.mean - lambda * r.variance;
    results_.push_back(std::move(r));
  }
  if (results_.empty()) return best;
  // Survivors first, as in SimBot; stable keeps SimBot's order on ties.
  std::stable_sort(results_.begin(), results_.end(), [](const RiskResult& a, const RiskResult& b) {
    return a.sim.pruned != b.sim.pruned ? !a.sim.pruned : a.utility > b.utility;
  });
  return results_.front().sim.move;
}

}  // namespace scrabble
