#pragma once

// Opponent-rack inference for a bot playing a game, shared by the self-play,
// match and evaluation apps: a RackFilter on a move model that follows every
// move of the game and feeds the sim bot's opponent racks.
//
// The game loop calls NewGame before the first move, then, for every move of
// either side, BeforeMove with the position and the move about to be played and
// AfterMove once it is applied.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "scrabble/equity.hpp"
#include "scrabble/features.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/inference.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/sim_bot.hpp"

namespace scrabble {

// Every rack explains every move: the posterior is the prior.
class UniformModel : public MoveModel {
 public:
  double Likelihood(const Observation&, const Rack&) override { return 1; }
};

// A program's move, softly: a rack explains the move by how close the move's
// static equity (score plus the leave it kept) is to the rack's best option,
// floor + exp(-gap / temperature), like Macondo's inference with a soft edge.
// The floor keeps a rack that cannot explain the move (a phony) possible.
class SoftRangeFinderModel : public MoveModel {
 public:
  SoftRangeFinderModel(const Gaddag& gaddag, const Leaves& leaves, double temperature,
                       double floor = 1e-3)
      : ranker_(gaddag, leaves), temperature_(temperature > 0 ? temperature : 4), floor_(floor) {}
  double Likelihood(const Observation& obs, const Rack& rack) override {
    const std::vector<Option> options = ranker_.Rank(obs, rack);
    if (options.empty()) return floor_;
    double gap = -1;
    for (int i : OptionRanker::Matches(obs, options)) {
      const double g = options.front().equity - options[i].equity;
      if (gap < 0 || g < gap) gap = g;
    }
    return gap < 0 ? floor_ : floor_ + std::exp(-gap / temperature_);
  }

 private:
  OptionRanker ranker_;
  double temperature_, floor_;
};

// The move models a tracker can use: "none" (uniform), "rangefinder" (or "rf"),
// "softrf" (tau: its temperature, 4 if 0) and "human" (needs the human model's
// params). Exits on an unknown name.
inline std::unique_ptr<MoveModel> MakeMoveModel(const std::string& name, const Gaddag& gaddag,
                                                const Leaves& leaves, const HumanModelParams* human,
                                                const FeatureLexicon* lexicon, double tau = 0) {
  if (name == "none") return std::make_unique<UniformModel>();
  if (name == "rangefinder" || name == "rf")
    return std::make_unique<RangeFinderModel>(gaddag, leaves, tau);
  if (name == "softrf") return std::make_unique<SoftRangeFinderModel>(gaddag, leaves, tau);
  if (name == "human") {
    if (!human) {
      std::cerr << "inference model 'human' needs --human-params\n";
      std::exit(2);
    }
    return std::make_unique<HumanMoveModel>(gaddag, leaves, *human, lexicon);
  }
  std::cerr << "unknown inference model: " << name << " (none, rangefinder or rf, softrf, human)\n";
  std::exit(2);
}

class RackTracker {
 public:
  RackTracker(std::unique_ptr<MoveModel> model, const Leaves& leaves, FilterParams params)
      : model_(std::move(model)), leaves_(leaves), params_(params) {
    filter_ = std::make_unique<RackFilter>(*model_, leaves_, params_);
  }
  RackTracker(const RackTracker&) = delete;
  RackTracker& operator=(const RackTracker&) = delete;

  // The sim bot whose opponent racks come from this tracker (may be null).
  void Attach(SimBot* sim) {
    sim_ = sim;
    if (sim_) sim_->set_rack_sampler(filter_.get());
  }

  // Restarts the filter's random stream from `seed`, as if just constructed with
  // it, so a game's tracking does not depend on the games played before it.
  void Reseed(std::uint64_t seed) {
    params_.seed = seed;
    filter_ = std::make_unique<RackFilter>(*model_, leaves_, params_);
    if (sim_) sim_->set_rack_sampler(filter_.get());
  }

  // A new game in which this side sits in `seat`.
  void NewGame(const Game& game, int seat) {
    seat_ = seat;
    turn_ = 0;
    exchanged_[0] = exchanged_[1] = false;
    pending_.reset();
    filter_->Reset(UnseenTiles(game.board(), game.rack(seat)), game.rack(1 - seat).Size());
  }

  // `m` is about to be played in `game` (by either side).
  void BeforeMove(const Game& game, const Move& m) {
    const int me = game.to_move(), opp = 1 - me;
    mover_ = me;
    Observation obs;
    obs.board = game.board();
    obs.move = m;
    obs.bag_size = game.bag_size();
    obs.own = game.rack(opp);
    obs.unseen = UnseenTiles(game.board(), game.rack(opp));
    obs.score_diff = game.score(me) - game.score(opp);
    obs.mover_is_bot = true;
    obs.turn = turn_++;
    obs.prev_exchange = exchanged_[me];
    exchanged_[me] = m.kind == Move::Kind::kExchange;
    if (m.kind == Move::Kind::kExchange) {
      // The observer only sees how many tiles went back.
      obs.exchanged = static_cast<int>(m.tiles.size());
      obs.move.tiles.clear();
    }
    pending_ = std::move(obs);
  }

  // The move given to BeforeMove has been applied: `game` is the position after it.
  void AfterMove(const Game& game) {
    if (!pending_ || game.over()) return;
    if (mover_ != seat_) {
      filter_->ObserveOpponent(*pending_);
      surprises_ += filter_->surprised();
    } else {
      filter_->Resync(UnseenTiles(game.board(), game.rack(seat_)), game.rack(1 - seat_).Size());
    }
    pending_.reset();
  }

  const RackFilter& filter() const { return *filter_; }
  // Opponent moves so far (over every game) that surprised the filter.
  long long surprises() const { return surprises_; }

 private:
  std::unique_ptr<MoveModel> model_;
  const Leaves& leaves_;
  FilterParams params_;
  std::unique_ptr<RackFilter> filter_;
  SimBot* sim_ = nullptr;
  int seat_ = 0, mover_ = 0, turn_ = 0;
  bool exchanged_[2] = {false, false};
  std::optional<Observation> pending_;
  long long surprises_ = 0;
};

}  // namespace scrabble
