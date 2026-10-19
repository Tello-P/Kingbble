#pragma once

#include <array>
#include <cstdint>
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
  explicit MoveGenerator(const Gaddag& gaddag) : gaddag_(&gaddag) {}

  // Generates with another lexicon from now on (e.g. one pruned for an
  // endgame). Call it again if the lexicon changes in place.
  void set_gaddag(const Gaddag& gaddag);

  // On an empty board a vertical opening is the transpose of a horizontal one
  // (same score, same premiums), so by default only horizontal openings are
  // generated, as Macondo does. Enable to get both orientations.
  void set_vertical_openings(bool on) {
    vertical_openings_ = on;
    prepared_ = false;
  }

  std::vector<Move> Generate(const Board& board, const Rack& rack);
  // Same, into `out` (cleared first), reusing its storage.
  void Generate(const Board& board, const Rack& rack, std::vector<Move>& out);

 private:
  struct Square {
    std::uint8_t letter;  // Board::kEmpty or letter (with blank bit)
    std::uint32_t cross;  // allowed letters (bit L for letter L); all if no cross word
    bool has_cross;       // tiles perpendicular to this square
    int cross_score;      // sum of perpendicular tile values
    bool anchor;
  };

  void Prepare(const Board& board);
  Square ComputeSquare(const Board& board, Direction dir, int line, int i);
  void GenLine(Direction dir, int line);
  void Gen(int pos, Gaddag::Node node);
  void GoOn(int pos, std::uint8_t letter, bool blank, Gaddag::Node next);
  void Record(int start, int end);
  void Take(Tile t) {
    rack_.Remove(t);
    if (rack_.Count(t) == 0) rack_letters_ &= ~(1u << t);
  }
  void PutBack(Tile t) {
    rack_.Add(t);
    rack_letters_ |= 1u << t;
  }

  struct CrossEntry {
    // Index of the square in the perpendicular word, its length - 1, then its
    // other letters.
    std::array<std::uint8_t, kBoardDim + 1> word{};
    std::uint32_t cross = 0;
    bool valid = false;
  };

  const Gaddag* gaddag_;
  bool vertical_openings_ = false;
  CrossEntry cross_cache_[2][kBoardDim][kBoardDim];
  // Square data by direction, line and index for prepared_board_.
  Square squares_[2][kBoardDim][kBoardDim];
  Board prepared_board_;
  bool prepared_ = false;
  // Per-line state.
  const Square* sq_ = nullptr;
  Direction dir_ = Direction::kHorizontal;
  int line_ = 0;
  int anchor_ = 0;
  int left_ = 0;  // leftmost square once the move turns right
  bool board_empty_ = false;
  // Per-call state.
  const Board* board_ = nullptr;
  Rack rack_;
  std::uint32_t rack_letters_ = 0;  // bit t set while the rack holds tile t
  char placed_[kBoardDim];          // '\0' = untouched square in the current partial move
  int tiles_placed_ = 0;
  std::vector<Move>* out_ = nullptr;
};

}  // namespace scrabble
