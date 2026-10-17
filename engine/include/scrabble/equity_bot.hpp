#pragma once

#include "scrabble/bot.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/movegen.hpp"

namespace scrabble {

// Static-evaluation player: picks the placement, exchange or pass with the
// highest StaticEquity; ties are broken by TiebreaksBetter.
class EquityBot : public Bot {
 public:
  EquityBot(const Gaddag& gaddag, const Leaves& leaves) : gen_(gaddag), leaves_(leaves) {}

  Move Choose(const Game& game) override;

 private:
  MoveGenerator gen_;
  const Leaves& leaves_;
};

}  // namespace scrabble
