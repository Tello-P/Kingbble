#pragma once

#include <array>
#include <cstdint>

#include "scrabble/bag.hpp"
#include "scrabble/board.hpp"
#include "scrabble/move.hpp"
#include "scrabble/rack.hpp"

namespace scrabble {

// Two-player game state under NASPA rules: going out earns twice the
// opponent's rack value; six consecutive scoreless turns end the game and
// each player loses their own rack value.
class Game {
 public:
  static constexpr int kMaxScorelessTurns = 6;
  static constexpr int kMinBagForExchange = 7;

  explicit Game(std::uint64_t seed);

  const Board& board() const { return board_; }
  const Rack& rack(int player) const { return racks_[player]; }
  int score(int player) const { return scores_[player]; }
  int to_move() const { return to_move_; }
  int bag_size() const { return bag_.Size(); }
  int scoreless_turns() const { return scoreless_turns_; }
  bool over() const { return over_; }

  // Applies a legal move for the player to move. Placement scores are
  // recomputed from the board, so `move.score` is not trusted.
  void Apply(const Move& move);

 private:
  void Finish();

  Board board_;
  Bag bag_;
  std::array<Rack, 2> racks_;
  std::array<int, 2> scores_{};
  int to_move_ = 0;
  int scoreless_turns_ = 0;
  bool over_ = false;
};

int RackValue(const Rack& rack);

}  // namespace scrabble
