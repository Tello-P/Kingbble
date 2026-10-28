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
  // lambda = lambda_scale * (lead + mean) / (2 sigma^2), sigma the spread still
  // to come (RestSpreadSd): the second-order term of E[Phi((lead + X) / sigma)].
  // A leader avoids variance and a trailer seeks it, more so as the bag empties.
  double lambda_scale = 1.0;
  // Risk window: with fewer or more tiles in the bag, or when SimBot's best
  // move wins with a chance below `decided` or above 1 - `decided`, RiskBot
  // plays SimBot's move.
  int min_bag = 7;
  int max_bag = 25;
  double decided = 0.02;
  // Variance of (candidate - SimBot's best) over the same iterations instead of
  // the candidate's own variance.
  bool relative_variance = false;
  // Each variance is shrunk toward the candidates' mean variance with this many
  // pseudo-iterations, so estimation noise does not pick the move.
  double shrink = 25;
  // Iterations per candidate inside the window, as a multiple of SimParams'.
  int iterations_factor = 1;
  WinParams win;
};

struct RiskResult {
  SimResult sim;
  double variance = 0;  // after shrinkage
  WinEstimate win;
  double lambda = 0;
  double utility = 0;
};

// Lambda of the mean-variance utility for an expected margin `lead_plus_mean`
// of the player to move after the playouts.
double RiskLambda(double lead_plus_mean, int bag_size, const RiskParams& params);

// True when the risk-aware utility applies to this position (bag window only;
// the decided check needs the simulation).
bool InRiskWindow(const Game& game, const RiskParams& params);

// Scores SimBot's results (best first, as SimBot sorts them) by the risk-aware
// utility, best first. Sets `active` to whether the utility decided the order;
// otherwise the utility is the mean and SimBot's order is kept.
std::vector<RiskResult> ScoreRisk(const Game& game, const std::vector<SimResult>& sims,
                                  const RiskParams& params, bool* active = nullptr);

// Picks among SimBot's candidates by a risk-aware utility instead of the mean
// spread inside the risk window, and plays like SimBot outside it. Inside the
// window the simulation does not prune (pruning drops candidates by their mean).
class RiskBot : public Bot {
 public:
  RiskBot(const Gaddag& gaddag, const Leaves& leaves, SimParams sim, RiskParams risk);

  Move Choose(const Game& game) override;
  // Candidates after the last Choose: survivors of SimBot's pruning first,
  // then by utility.
  const std::vector<RiskResult>& last_results() const { return results_; }
  // Whether the risk-aware utility picked the last move.
  bool last_active() const { return active_; }
  void set_rack_sampler(const RackSampler* sampler) {
    sim_.set_rack_sampler(sampler);
    risk_sim_.set_rack_sampler(sampler);
  }
  void Reseed(std::uint64_t seed) {
    sim_.Reseed(seed);
    risk_sim_.Reseed(seed);
  }

 private:
  static SimParams RiskSimParams(SimParams sim, const RiskParams& risk);

  SimBot sim_;       // outside the window
  SimBot risk_sim_;  // inside: no pruning, iterations_factor times the iterations
  RiskParams params_;
  std::vector<RiskResult> results_;
  bool active_ = false;
};

}  // namespace scrabble
