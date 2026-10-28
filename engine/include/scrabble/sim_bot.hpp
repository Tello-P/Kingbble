#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <random>
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
  // Moves played after the candidate in each playout, alternating opponent and
  // us, each the mover's best by static equity (2: the opponent's reply and our
  // follow-up). 3 won 56.5 % of 200 games against 2 on the #57 position bank.
  int plies = 3;
  // Seconds per move (0: no limit). Batches stop once it is spent, after
  // min_iterations, so results then depend on the machine's speed.
  double time_limit_s = 0;
  // Replies inside the playouts: with reply_temperature > 0 the mover picks
  // among their reply_top best static moves with probability proportional to
  // exp((equity - best) / reply_temperature) instead of always the best. Only
  // the opponent's replies, unless reply_own.
  int reply_top = 1;
  double reply_temperature = 0;
  bool reply_own = false;
};

// Picks the move of the player to move inside a playout (e.g. from a model of
// how opponents play). One per thread.
class ReplyPolicy {
 public:
  virtual ~ReplyPolicy() = default;
  virtual Move Pick(const Game& game, std::mt19937_64& rng) = 0;
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

// Monte Carlo simulation (as Quackle and Macondo do): for each of the top
// static candidates, deal the opponent a random rack from the unseen tiles,
// then play `plies` more moves, opponent and us in turn, each the mover's best
// static move. A candidate's value is the spread over those moves plus the
// leave of the last mover, ours counted for us and the opponent's against. Every candidate sees the
// same racks and draws in a given iteration, and results do not depend on the number of threads.
class SimBot : public Bot {
 public:
  SimBot(const Gaddag& gaddag, const Leaves& leaves, SimParams params);

  Move Choose(const Game& game) override;
  // Candidates after the last Choose, best first.
  const std::vector<SimResult>& last_results() const { return results_; }
  // Opponent racks inferred from the game so far (e.g. a RackFilter); nullptr
  // for uniform racks only. It must outlive Choose and stay unchanged during it.
  void set_rack_sampler(const RackSampler* sampler) { sampler_ = sampler; }
  // Restarts the random streams from `seed`, as if just constructed with it
  // (e.g. so two bots get the same streams in the same seat of a paired game).
  void Reseed(std::uint64_t seed) {
    params_.seed = seed;
    calls_ = 0;
  }
  // Replies from `make()` policies (one per thread) instead of the static
  // ones, for the same plies as reply_top; an empty function goes back.
  void set_reply_policy(const std::function<std::unique_ptr<ReplyPolicy>()>& make);

 private:
  double Playout(int thread, const Game& base, const Move& candidate, std::uint64_t seed) const;
  Move Reply(int thread, const Game& game, std::mt19937_64& rng) const;
  Game SamplePosition(const Game& game, std::uint64_t seed) const;

  SimParams params_;
  const RackSampler* sampler_ = nullptr;
  std::vector<std::unique_ptr<EquityBot>> bots_;        // one per thread
  std::vector<std::unique_ptr<ReplyPolicy>> policies_;  // one per thread, or none
  std::uint64_t calls_ = 0;
  std::vector<SimResult> results_;
};

}  // namespace scrabble
