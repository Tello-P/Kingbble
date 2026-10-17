#include "scrabble/tiles.hpp"

#include <gtest/gtest.h>

namespace scrabble {

TEST(Tiles, EnglishDistributionHas100Tiles) { EXPECT_EQ(TotalTiles(), 100); }

TEST(Tiles, KnownScores) {
  const auto& t = EnglishTiles();
  EXPECT_EQ(t[*TileFromChar('Q')].score, 10);
  EXPECT_EQ(t[*TileFromChar('Z')].score, 10);
  EXPECT_EQ(t[*TileFromChar('E')].count, 12);
  EXPECT_EQ(t[kBlank].count, 2);
  EXPECT_EQ(t[kBlank].score, 0);
}

TEST(Tiles, CharRoundTrip) {
  for (char c : std::string("?ABCXYZ")) EXPECT_EQ(TileToChar(*TileFromChar(c)), c);
  EXPECT_EQ(TileFromChar('q'), TileFromChar('Q'));
  EXPECT_FALSE(TileFromChar('1').has_value());
}

}  // namespace scrabble
