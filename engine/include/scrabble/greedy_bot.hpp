#pragma once

#include "scrabble/game.hpp"
#include "scrabble/movegen.hpp"

namespace scrabble {

// Baseline player: always takes the highest-scoring placement. With no
// placement it exchanges the whole rack if allowed, otherwise passes.
class GreedyBot {
 public:
  explicit GreedyBot(const Gaddag& gaddag) : gen_(gaddag) {}

  Move Choose(const Game& game);
  // Number of moves generated on the last call (for benchmarks).
  std::size_t last_num_moves() const { return last_num_moves_; }

 private:
  MoveGenerator gen_;
  std::size_t last_num_moves_ = 0;
};

}  // namespace scrabble
