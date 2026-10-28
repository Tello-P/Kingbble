#pragma once

#include <vector>

#include "scrabble/bot.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/movegen.hpp"

namespace scrabble {

struct RankedMove {
  Move move;
  double equity = 0;
};

// Static-evaluation player: picks the placement, exchange or pass with the
// highest StaticEquity; ties are broken by TiebreaksBetter.
class EquityBot : public Bot {
 public:
  EquityBot(const Gaddag& gaddag, const Leaves& leaves) : gen_(gaddag), leaves_(leaves) {}

  Move Choose(const Game& game) override;
  // Every option for the player to move, best first.
  std::vector<RankedMove> Rank(const Game& game);

  const Leaves& leaves() const { return leaves_; }
  // Late leave values with 1..n tiles in the bag (see LeaveValue); 0: off.
  void set_late_bag(int n) { late_bag_ = n; }
  int late_bag() const { return late_bag_; }

 private:
  std::vector<Move> Options(const Game& game);

  MoveGenerator gen_;
  const Leaves& leaves_;
  int late_bag_ = 0;
};

}  // namespace scrabble
