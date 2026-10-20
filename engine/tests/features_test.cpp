#include "scrabble/features.hpp"

#include <gtest/gtest.h>

#include <string>

#include "scrabble/equity.hpp"
#include "scrabble/gaddag.hpp"

namespace scrabble {

namespace {

std::string Fixture(const std::string& name) { return std::string(KINGBBLE_FIXTURES) + "/" + name; }

}  // namespace

TEST(Features, ReplayTurnsNumbersMovesAndFlagsPhonies) {
  std::string error;
  auto game = ReadGcgFile(Fixture("sample.gcg"), &error);
  ASSERT_TRUE(game) << error;
  std::vector<ReplayedTurn> turns = ReplayTurns(*game, &error);
  ASSERT_EQ(turns.size(), 8u) << error;
  EXPECT_EQ(turns[0].bag_size, 86);
  EXPECT_TRUE(turns[0].known_rack);
  EXPECT_EQ(turns[0].rack.ToString(), "AERTXYZ");
  // RATE is on the board before bob's RATES: 4 tiles out of the bag.
  EXPECT_EQ(turns[1].bag_size, 82);
  EXPECT_FALSE(turns[1].board.IsEmpty(7, 4));
  EXPECT_EQ(turns[1].move.tiles, "....S");
  EXPECT_TRUE(turns[4].phony);  // (S)OX, withdrawn
  EXPECT_FALSE(turns[5].phony);
  EXPECT_FALSE(turns[7].known_rack);  // -3: unknown tiles
  EXPECT_EQ(turns[7].turn, 7);
}

TEST(Features, RankOptionsIsSortedAndFindsThePlayedMove) {
  Gaddag gaddag = Gaddag::FromWords({"AT", "TA", "EAT", "TEA", "ATE", "RATE", "TEAR", "RATES"});
  Leaves leaves = Leaves::FromFile(Fixture("leaves_small.klv2"));
  MoveGenerator gen(gaddag);
  Board board;
  Rack rack = *Rack::FromString("AERTXYZ");
  auto options = RankOptions(gen, board, rack, 86, UnseenTiles(board, rack), leaves);
  ASSERT_GT(options.size(), 3u);
  EXPECT_EQ(options.front().features[kFeatGap], 0.f);
  EXPECT_EQ(options.front().features[kFeatLogRank], 0.f);
  for (std::size_t i = 1; i < options.size(); ++i) {
    EXPECT_GE(options[i - 1].equity, options[i].equity);
    EXPECT_GE(options[i].features[kFeatGap], 0.f);
  }
  Move rate;
  rate.kind = Move::Kind::kPlace;
  rate.row = 7;
  rate.col = 4;
  rate.tiles = "RATE";
  const int i = FindOption(options, rate);
  ASSERT_GE(i, 0);
  EXPECT_EQ(options[i].features[kFeatTiles], 4.f);
  EXPECT_EQ(options[i].features[kFeatScore], 8.f);
  Move pass;
  const int p = FindOption(options, pass);
  ASSERT_GE(p, 0);
  EXPECT_EQ(options[p].features[kFeatPass], 1.f);
  Move exchange;
  exchange.kind = Move::Kind::kExchange;
  exchange.tiles = "ZX";
  const int x = FindOption(options, exchange);
  ASSERT_GE(x, 0);
  EXPECT_EQ(options[x].features[kFeatExchange], 1.f);
  Move phony = rate;
  phony.tiles = "TARE";
  EXPECT_EQ(FindOption(options, phony), -1);
}

TEST(Features, OneTilePlaysMatchInEitherDirection) {
  Move a;
  a.kind = Move::Kind::kPlace;
  a.row = 6;
  a.col = 7;
  a.tiles = "A.";
  Move b = a;
  b.dir = Direction::kVertical;
  b.tiles = "A";
  EXPECT_TRUE(SamePlay(a, b));
  b.col = 8;
  EXPECT_FALSE(SamePlay(a, b));
}

}  // namespace scrabble
