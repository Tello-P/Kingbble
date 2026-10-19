#pragma once

#include <memory>

#include "scrabble/approx_preendgame.hpp"
#include "scrabble/bot.hpp"

namespace scrabble {

// Plays like `inner` except with min_bag..max_bag tiles in the bag, where it
// plays the approximate pre-endgame's best play (`inner` again if the time
// limit left no sample).
class ApproxPreEndgameBot : public Bot {
 public:
  ApproxPreEndgameBot(std::unique_ptr<Bot> inner, const Gaddag& gaddag, const Leaves& leaves,
                      int min_bag, int max_bag, ApproxPreEndgameParams params)
      : inner_(std::move(inner)),
        solver_(gaddag, leaves),
        min_bag_(min_bag),
        max_bag_(max_bag),
        params_(params) {}

  Move Choose(const Game& game) override;
  // Plays of the last approximate decision, best first (empty if `inner` played).
  const std::vector<ApproxPreEndgamePlay>& last_plays() const { return last_; }

 private:
  std::unique_ptr<Bot> inner_;
  ApproxPreEndgameSolver solver_;
  int min_bag_;
  int max_bag_;
  ApproxPreEndgameParams params_;
  std::uint64_t calls_ = 0;
  std::vector<ApproxPreEndgamePlay> last_;
};

}  // namespace scrabble
