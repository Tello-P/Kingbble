#include "scrabble/board.hpp"

#include <cassert>

namespace scrabble {

namespace {

// Top-left quadrant (rows 0..7, columns 0..7); the rest is symmetric.
// T = triple word, D = double word, t = triple letter, d = double letter.
constexpr const char* kQuadrant[8] = {
    "T..d...T",  //
    ".D...t..",  //
    "..D...d.",  //
    "d..D...d",  //
    "....D...",  //
    ".t...t..",  //
    "..d...d.",  //
    "T..d...D",  // center = double word
};

}  // namespace

Board::Board() {
  for (auto& row : cells_) row.fill(kEmpty);
}

Bonus Board::BonusAt(int row, int col) {
  assert(row >= 0 && row < kBoardDim && col >= 0 && col < kBoardDim);
  int r = row <= kCenter ? row : kBoardDim - 1 - row;
  int c = col <= kCenter ? col : kBoardDim - 1 - col;
  switch (kQuadrant[r][c]) {
    case 'T':
      return Bonus::kTripleWord;
    case 'D':
      return Bonus::kDoubleWord;
    case 't':
      return Bonus::kTripleLetter;
    case 'd':
      return Bonus::kDoubleLetter;
    default:
      return Bonus::kNone;
  }
}

void Board::Place(int row, int col, Tile letter, bool from_blank) {
  assert(letter >= 1 && letter <= 26);
  cells_[row][col] = static_cast<std::uint8_t>(letter | (from_blank ? kBlankBit : 0));
}

}  // namespace scrabble
