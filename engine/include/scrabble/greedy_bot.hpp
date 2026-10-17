#pragma once

#include "scrabble/bot.hpp"
#include "scrabble/movegen.hpp"

namespace scrabble {

// Baseline player: always takes the highest-scoring placement. With no
// placement it exchanges the whole rack if allowed, otherwise passes.
class GreedyBot : public Bot {
 public:
  explicit GreedyBot(const Gaddag& gaddag) : gen_(gaddag) {}

  Move Choose(const Game& game) override;

 private:
  MoveGenerator gen_;
};

}  // namespace scrabble
