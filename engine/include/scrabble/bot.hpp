#pragma once

#include <cstddef>

#include "scrabble/game.hpp"
#include "scrabble/move.hpp"

namespace scrabble {

// A player that picks a move for the player to move. Bots only look at the
// board, the scores, the bag size and their own rack.
class Bot {
 public:
  virtual ~Bot() = default;

  virtual Move Choose(const Game& game) = 0;
  // Number of moves generated on the last call (for benchmarks).
  std::size_t last_num_moves() const { return last_num_moves_; }

 protected:
  std::size_t last_num_moves_ = 0;
};

}  // namespace scrabble
