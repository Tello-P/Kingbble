#pragma once

#include <memory>

#include "scrabble/bot.hpp"
#include "scrabble/preendgame.hpp"

namespace scrabble {

// Plays like `inner` except with 1..max_bag tiles in the bag, where it plays the
// pre-endgame solver's best play. If the solver runs out of time, the best of
// the plays it finished is taken; if it finished none, `inner` plays.
class PreEndgameBot : public Bot {
 public:
  PreEndgameBot(std::unique_ptr<Bot> inner, const Gaddag& gaddag, const Leaves& leaves, int max_bag,
                PreEndgameParams params)
      : inner_(std::move(inner)), solver_(gaddag, leaves), max_bag_(max_bag), params_(params) {}

  Move Choose(const Game& game) override;
  // Solution of the last pre-endgame decision (empty plays if `inner` played).
  const PreEndgameResult& last_result() const { return last_; }

 private:
  std::unique_ptr<Bot> inner_;
  PreEndgameSolver solver_;
  int max_bag_;
  PreEndgameParams params_;
  PreEndgameResult last_;
};

}  // namespace scrabble
