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

 private:
  std::vector<Move> Options(const Game& game);

  MoveGenerator gen_;
  const Leaves& leaves_;
};

}  // namespace scrabble
