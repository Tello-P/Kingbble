#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "scrabble/bot.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/inference.hpp"

namespace scrabble {

struct SimParams {
  int candidates = 10;       // top static-equity moves to simulate
  int max_iterations = 200;  // simulated opponent racks per candidate
  int min_iterations = 32;   // before any pruning
  int batch = 16;            // iterations between pruning checks
  double prune_z = 2.58;     // drop candidates this many standard errors behind
  int threads = 1;
  std::uint64_t seed = 1;
  // Share of opponent racks taken from the rack sampler, if one is set; the
  // rest are uniform draws from the unseen tiles.
  double posterior_share = 0.8;
};

struct SimResult {
  Move move;
  double static_equity = 0;
  double mean = 0;       // mean simulated value
  double std_error = 0;  // standard error of the mean
  int iterations = 0;
  bool pruned = false;
  std::vector<double> values;  // simulated value of each iteration
};

// Monte Carlo 2-ply simulation (as Quackle and Macondo do): for each of the top
// static candidates, deal the opponent a random rack from the unseen tiles,
// let them reply with their best static move, then play our best static
// follow-up. A candidate's value is the spread over the three moves plus our
// final leave. Every candidate sees the same racks and draws in a given
// iteration, and results do not depend on the number of threads.
class SimBot : public Bot {
 public:
  SimBot(const Gaddag& gaddag, const Leaves& leaves, SimParams params);

  Move Choose(const Game& game) override;
  // Candidates after the last Choose, best first.
  const std::vector<SimResult>& last_results() const { return results_; }
  // Opponent racks inferred from the game so far (e.g. a RackFilter); nullptr
  // for uniform racks only. It must outlive Choose and stay unchanged during it.
  void set_rack_sampler(const RackSampler* sampler) { sampler_ = sampler; }

 private:
  double Playout(EquityBot& bot, const Game& base, const Move& candidate) const;
  Game SamplePosition(const Game& game, std::uint64_t seed) const;

  SimParams params_;
  const RackSampler* sampler_ = nullptr;
  std::vector<std::unique_ptr<EquityBot>> bots_;  // one per thread
  std::uint64_t calls_ = 0;
  std::vector<SimResult> results_;
};

}  // namespace scrabble
