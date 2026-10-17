#pragma once

#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/move.hpp"
#include "scrabble/rack.hpp"

namespace scrabble {

inline constexpr int kBingoBonus = 50;

// Scores a placement move against the board it would be played on.
// The move must be legal; its `score` field is ignored.
int ScoreMove(const Board& board, const Move& move);

// Generates every legal placement for `rack` (Gordon's GADDAG algorithm).
// Each move appears once; one-tile plays are reported in a single direction.
class MoveGenerator {
 public:
  explicit MoveGenerator(const Gaddag& gaddag) : gaddag_(gaddag) {}

  // On an empty board a vertical opening is the transpose of a horizontal one
  // (same score, same premiums), so by default only horizontal openings are
  // generated, as Macondo does. Enable to get both orientations.
  void set_vertical_openings(bool on) { vertical_openings_ = on; }

  std::vector<Move> Generate(const Board& board, const Rack& rack);

 private:
  struct Square {
    std::uint8_t letter;  // Board::kEmpty or letter (with blank bit)
    std::uint32_t cross;  // allowed letters (bit L for letter L); all if no cross word
    bool has_cross;       // tiles perpendicular to this square
    int cross_score;      // sum of perpendicular tile values
    bool anchor;
  };

  void PrepareLine(const Board& board, Direction dir, int line);
  void GenLine(Direction dir, int line);
  void Gen(int pos, Gaddag::Node node);
  void GoOn(int pos, std::uint8_t letter, bool blank, Gaddag::Node next);
  void Record(int start, int end);

  const Gaddag& gaddag_;
  bool vertical_openings_ = false;
  // Per-line state.
  Square sq_[kBoardDim];
  Direction dir_ = Direction::kHorizontal;
  int line_ = 0;
  int anchor_ = 0;
  int left_ = 0;  // leftmost square once the move turns right
  bool board_empty_ = false;
  // Per-call state.
  const Board* board_ = nullptr;
  Rack rack_;
  char placed_[kBoardDim];  // '\0' = untouched square in the current partial move
  int tiles_placed_ = 0;
  std::vector<Move>* out_ = nullptr;
};

}  // namespace scrabble
