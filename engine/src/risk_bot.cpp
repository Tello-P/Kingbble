#include "scrabble/risk_bot.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace scrabble {

namespace {

// Sample variance of a[i] - b[i] over the first n iterations (b may be null).
double Variance(const std::vector<double>& a, const std::vector<double>* b, std::size_t n) {
  if (n < 2) return 0;
  double sum = 0, sq = 0;
  for (std::size_t i = 0; i < n; ++i) sum += a[i] - (b ? (*b)[i] : 0);
  const double mean = sum / n;
  for (std::size_t i = 0; i < n; ++i) {
    const double d = a[i] - (b ? (*b)[i] : 0) - mean;
    sq += d * d;
  }
  return sq / (n - 1);
}

}  // namespace

double PolicyScale(const RiskParams& params, int bag_size, double lead) {
  for (const LambdaRule& r : params.policy)
    if (bag_size >= r.bag_lo && bag_size <= r.bag_hi && lead >= r.lead_lo && lead <= r.lead_hi)
      return r.scale;
  return params.lambda_scale;
}

std::vector<LambdaRule> LoadRiskPolicy(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open risk policy: " + path);
  std::vector<LambdaRule> rules;
  std::string line;
  int number = 0;
  while (std::getline(in, line)) {
    ++number;
    if (const auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
    std::istringstream ss(line);
    LambdaRule r;
    if (!(ss >> r.bag_lo)) continue;  // blank or comment
    if (!(ss >> r.bag_hi >> r.lead_lo >> r.lead_hi >> r.scale))
      throw std::runtime_error("malformed risk policy, line " + std::to_string(number));
    rules.push_back(r);
  }
  return rules;
}

double RiskLambda(double lead_plus_mean, int bag_size, const RiskParams& params, double lead) {
  const double sd = RestSpreadSd(bag_size, params.win);
  if (sd <= 0) return 0;
  return PolicyScale(params, bag_size, lead) * lead_plus_mean / (2 * sd * sd);
}

bool InRiskWindow(const Game& game, const RiskParams& params) {
  return game.bag_size() >= params.min_bag && game.bag_size() <= params.max_bag;
}

std::vector<RiskResult> ScoreRisk(const Game& game, const std::vector<SimResult>& sims,
                                  const RiskParams& params, bool* active) {
  std::vector<RiskResult> out;
  if (active) *active = false;
  if (sims.empty()) return out;
  const int me = game.to_move();
  const double lead = game.score(me) - game.score(1 - me);
  const std::vector<WinEstimate> wins = WinProbabilities(game, sims, params.win);
  const std::vector<double>& best = sims.front().values;

  std::vector<double> raw(sims.size());
  double pool = 0;
  int pooled = 0;
  for (std::size_t c = 0; c < sims.size(); ++c) {
    const std::vector<double>& v = sims[c].values;
    raw[c] = params.relative_variance ? Variance(v, &best, std::min(v.size(), best.size()))
                                      : Variance(v, nullptr, v.size());
    if (v.size() > 1) pool += raw[c], ++pooled;
  }
  if (pooled) pool /= pooled;

  const bool on = InRiskWindow(game, params) && wins.front().win >= params.decided &&
                  wins.front().win <= 1 - params.decided;
  if (active) *active = on;
  for (std::size_t c = 0; c < sims.size(); ++c) {
    const SimResult& s = sims[c];
    const double k = static_cast<double>(s.values.size());
    RiskResult r;
    r.sim = s;
    r.variance =
        k + params.shrink > 0 ? (k * raw[c] + params.shrink * pool) / (k + params.shrink) : raw[c];
    r.win = wins[c];
    r.utility = s.mean;
    if (on && params.mode == RiskMode::kWinProb) {
      r.utility = r.win.win;
    } else if (on) {
      r.lambda = RiskLambda(lead + s.mean, game.bag_size(), params, lead);
      r.utility = s.mean - r.lambda * r.variance;
    }
    out.push_back(std::move(r));
  }
  // Survivors first, as in SimBot; stable keeps SimBot's order on ties.
  std::stable_sort(out.begin(), out.end(), [](const RiskResult& a, const RiskResult& b) {
    return a.sim.pruned != b.sim.pruned ? !a.sim.pruned : a.utility > b.utility;
  });
  return out;
}

SimParams RiskBot::RiskSimParams(SimParams sim, const RiskParams& risk) {
  sim.prune_z = 1e9;
  sim.max_iterations *= std::max(1, risk.iterations_factor);
  return sim;
}

RiskBot::RiskBot(const Gaddag& gaddag, const Leaves& leaves, SimParams sim, RiskParams risk)
    : sim_(gaddag, leaves, sim),
      risk_sim_(gaddag, leaves, RiskSimParams(sim, risk)),
      params_(risk) {}

Move RiskBot::Choose(const Game& game) {
  SimBot& bot = InRiskWindow(game, params_) ? risk_sim_ : sim_;
  Move best = bot.Choose(game);
  last_num_moves_ = bot.last_num_moves();
  results_ = ScoreRisk(game, bot.last_results(), params_, &active_);
  return results_.empty() ? best : results_.front().sim.move;
}

}  // namespace scrabble
