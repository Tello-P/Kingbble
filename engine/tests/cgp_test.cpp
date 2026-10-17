#include "scrabble/cgp.hpp"

#include <gtest/gtest.h>

namespace scrabble {

TEST(Cgp, EmptyBoard) {
  std::string rows = "15";
  for (int r = 1; r < kBoardDim; ++r) rows += "/15";
  EXPECT_EQ(BoardToCgp(Board()), rows);
}

TEST(Cgp, LettersBlanksAndRuns) {
  Board b;
  b.Place(7, 7, *TileFromChar('Q'), false);
  b.Place(7, 8, *TileFromChar('I'), true);
  b.Place(0, 14, *TileFromChar('Z'), false);
  std::string cgp = BoardToCgp(b);
  EXPECT_EQ(cgp.substr(0, 3), "14Z");
  EXPECT_NE(cgp.find("/7Qi6/"), std::string::npos);
}

TEST(Cgp, GameLine) {
  Game g(3);
  std::string cgp = GameToCgp(g, "ENABLE");
  std::string tail = " " + g.rack(0).ToString() + "/" + g.rack(1).ToString() + " 0/0 0 lex ENABLE;";
  ASSERT_GE(cgp.size(), tail.size());
  EXPECT_EQ(cgp.substr(cgp.size() - tail.size()), tail);
}

}  // namespace scrabble
