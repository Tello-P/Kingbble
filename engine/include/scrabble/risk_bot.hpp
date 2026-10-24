#pragma once

#include <vector>

#include "scrabble/bot.hpp"
#include "scrabble/sim_bot.hpp"
#include "scrabble/win_prob.hpp"

namespace scrabble {

enum class RiskMode {
  kMeanVariance,  // mean - lambda * variance of the simulated spread
  kWinProb,       // estimated probability of winning
};

struct RiskParams {
  RiskMode mode = RiskMode::kMeanVariance;
  // lambda = max_lambda * clamp(lead / lead_scale, -1, 1) * (1 - bag / full_bag):
  // a leader avoids variance and a trailer seeks it, more so as the bag empties.
  double max_lambda = 0.01;
  double lead_scale = 100.0;
  int full_bag = 86;
  WinParams win;
};

struct RiskResult {
  SimResult sim;
  double variance = 0;  // sample variance of the simulated values
  WinEstimate win;
  double utility = 0;
};

// Lambda of the mean-variance utility for a lead of the player to move.
double RiskLambda(double lead, int bag_size, const RiskParams& params);

// Picks among SimBot's candidates by a risk-aware utility instead of the mean
// spread. SimBot's pruning drops candidates by their mean, so a large
// `prune_z` keeps risky candidates in the running.
class RiskBot : public Bot {
 public:
  RiskBot(const Gaddag& gaddag, const Leaves& leaves, SimParams sim, RiskParams risk);

  Move Choose(const Game& game) override;
  // Candidates after the last Choose: survivors of SimBot's pruning first,
  // then by utility.
  const std::vector<RiskResult>& last_results() const { return results_; }
  void set_rack_sampler(const RackSampler* sampler) { sim_.set_rack_sampler(sampler); }

 private:
  SimBot sim_;
  RiskParams params_;
  std::vector<RiskResult> results_;
};

}  // namespace scrabble
