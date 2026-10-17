#include "scrabble/board.hpp"

#include <gtest/gtest.h>

namespace scrabble {

TEST(Board, CenterIsDoubleWord) { EXPECT_EQ(Board::BonusAt(7, 7), Bonus::kDoubleWord); }

TEST(Board, CornersAreTripleWord) {
  for (int r : {0, 14})
    for (int c : {0, 14}) EXPECT_EQ(Board::BonusAt(r, c), Bonus::kTripleWord);
}

TEST(Board, PremiumCountsMatchStandardBoard) {
  int dl = 0, tl = 0, dw = 0, tw = 0;
  for (int r = 0; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c) switch (Board::BonusAt(r, c)) {
        case Bonus::kDoubleLetter:
          ++dl;
          break;
        case Bonus::kTripleLetter:
          ++tl;
          break;
        case Bonus::kDoubleWord:
          ++dw;
          break;
        case Bonus::kTripleWord:
          ++tw;
          break;
        default:
          break;
      }
  EXPECT_EQ(dl, 24);
  EXPECT_EQ(tl, 12);
  EXPECT_EQ(dw, 17);  // 16 + center
  EXPECT_EQ(tw, 8);
}

TEST(Board, PlaceBlank) {
  Board b;
  EXPECT_TRUE(b.IsEmpty(7, 7));
  b.Place(7, 7, *TileFromChar('E'), true);
  EXPECT_FALSE(b.IsEmpty(7, 7));
  EXPECT_EQ(b.At(7, 7), 5 | Board::kBlankBit);
}

}  // namespace scrabble
