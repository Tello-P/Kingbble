#pragma once

#include <array>
#include <cstdint>

#include "scrabble/tiles.hpp"

namespace scrabble {

inline constexpr int kBoardDim = 15;
inline constexpr int kCenter = 7;

enum class Bonus : std::uint8_t { kNone, kDoubleLetter, kTripleLetter, kDoubleWord, kTripleWord };

// Standard 15x15 board. An empty square holds kEmpty; a placed tile
// stores the letter (1..26) and, if it came from a blank, the kBlankBit bit.
class Board {
 public:
  static constexpr std::uint8_t kEmpty = 0xFF;
  static constexpr std::uint8_t kBlankBit = 0x80;

  Board();

  static Bonus BonusAt(int row, int col);

  bool IsEmpty(int row, int col) const { return cells_[row][col] == kEmpty; }
  std::uint8_t At(int row, int col) const { return cells_[row][col]; }
  void Place(int row, int col, Tile letter, bool from_blank);
  void Clear(int row, int col) { cells_[row][col] = kEmpty; }

 private:
  std::array<std::array<std::uint8_t, kBoardDim>, kBoardDim> cells_;
};

}  // namespace scrabble
