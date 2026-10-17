#include "scrabble/equity.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>

#include "scrabble/equity_bot.hpp"
#include "scrabble/game.hpp"
#include "scrabble/greedy_bot.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

Move Place(int row, int col, Direction dir, std::string tiles, int score) {
  Move m;
  m.kind = Move::Kind::kPlace;
  m.row = row;
  m.col = col;
  m.dir = dir;
  m.tiles = std::move(tiles);
  m.score = score;
  return m;
}

Move Exchange(std::string tiles) {
  Move m;
  m.kind = Move::Kind::kExchange;
  m.tiles = std::move(tiles);
  return m;
}

}  // namespace

TEST(Equity, UnseenTilesExcludeBoardAndOwnRack) {
  Board b;
  b.Place(7, 7, *TileFromChar('Q'), false);
  b.Place(7, 8, *TileFromChar('I'), true);
  Rack own = *Rack::FromString("?ZEE");
  Rack unseen = UnseenTiles(b, own);
  EXPECT_EQ(unseen.Size(), TotalTiles() - 6);
  EXPECT_EQ(unseen.Count(kBlank), 0);
  EXPECT_EQ(unseen.Count(*TileFromChar('Q')), 0);
  EXPECT_EQ(unseen.Count(*TileFromChar('Z')), 0);
  EXPECT_EQ(unseen.Count(*TileFromChar('I')), 9);
  EXPECT_EQ(unseen.Count(*TileFromChar('E')), 10);
}

TEST(Equity, LeaveAfterRemovesPlayedTilesAndBlanks) {
  Rack rack = *Rack::FromString("?AEQSTU");
  EXPECT_EQ(LeaveAfter(rack, Place(7, 7, Direction::kHorizontal, "QaT", 0)).ToString(), "AESU");
  EXPECT_EQ(LeaveAfter(rack, Place(7, 7, Direction::kVertical, "Q.T", 0)).ToString(), "?AESU");
  EXPECT_EQ(LeaveAfter(rack, Exchange("?Q")).ToString(), "AESTU");
  EXPECT_EQ(LeaveAfter(rack, Move{}).ToString(), "?AEQSTU");
}

TEST(Equity, ExchangesAreDistinctSubsets) {
  std::vector<Move> ex = GenerateExchanges(*Rack::FromString("AAB"));
  std::set<std::string> tiles;
  for (const Move& m : ex) {
    EXPECT_EQ(m.kind, Move::Kind::kExchange);
    tiles.insert(m.tiles);
  }
  EXPECT_EQ(tiles, (std::set<std::string>{"A", "AA", "AAB", "AB", "B"}));
  EXPECT_EQ(ex.size(), 5u);
  EXPECT_EQ(GenerateExchanges(*Rack::FromString("?AEINST")).size(), 127u);
  EXPECT_TRUE(GenerateExchanges(Rack{}).empty());
}

TEST(Equity, StaticEquityAddsLeaveWhileTheBagHasTiles) {
  Leaves l = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  Rack rack = *Rack::FromString("?ADGOQS");
  Rack unseen;
  Move dog = Place(7, 7, Direction::kHorizontal, "DOG", 10);
  EXPECT_DOUBLE_EQ(StaticEquity(dog, rack, 50, unseen, l), 10);  // ?AQS not in table
  Move dogas = Place(7, 7, Direction::kHorizontal, "DOGAQ", 30);
  EXPECT_DOUBLE_EQ(StaticEquity(dogas, rack, 50, unseen, l), 30 + 30);  // keeps ?S
  EXPECT_DOUBLE_EQ(StaticEquity(Exchange("ADGOQS"), rack, 50, unseen, l), 25.5);
  EXPECT_DOUBLE_EQ(StaticEquity(Exchange("?ADGOS"), rack, 50, unseen, l), -7.75);
}

TEST(Equity, StaticEquityWithEmptyBag) {
  Leaves l = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  Rack rack = *Rack::FromString("DOGQ");
  Rack unseen = *Rack::FromString("ZA");  // 11 points
  EXPECT_DOUBLE_EQ(StaticEquity(Place(0, 0, Direction::kHorizontal, "DOG", 5), rack, 0, unseen, l),
                   5 - 2 * 10 - 10);
  Rack dog = *Rack::FromString("DOG");
  EXPECT_DOUBLE_EQ(StaticEquity(Place(0, 0, Direction::kHorizontal, "DOG", 5), dog, 0, unseen, l),
                   5 + 2 * 11);
}

TEST(Move, TiebreaksLikeMacondo) {
  Move a = Place(7, 7, Direction::kHorizontal, "CAT", 10);
  EXPECT_TRUE(TiebreaksBetter(Place(9, 9, Direction::kVertical, "DOGS", 11), a));
  EXPECT_TRUE(TiebreaksBetter(a, Exchange("Q")));
  EXPECT_TRUE(TiebreaksBetter(Exchange("Q"), Move{}));
  EXPECT_TRUE(TiebreaksBetter(Place(6, 9, Direction::kHorizontal, "CAT", 10), a));
  EXPECT_TRUE(TiebreaksBetter(Place(7, 6, Direction::kHorizontal, "CAT", 10), a));
  EXPECT_TRUE(TiebreaksBetter(a, Place(7, 7, Direction::kVertical, "CAT", 10)));
  EXPECT_TRUE(TiebreaksBetter(Place(7, 7, Direction::kHorizontal, "C.T", 10), a));
  EXPECT_TRUE(TiebreaksBetter(Place(7, 7, Direction::kHorizontal, "BAT", 10), a));
  EXPECT_TRUE(TiebreaksBetter(a, Place(7, 7, Direction::kHorizontal, "cAT", 10)));
  EXPECT_TRUE(TiebreaksBetter(Exchange("A"), Exchange("AB")));
  EXPECT_FALSE(TiebreaksBetter(a, a));
}

TEST(EquityBot, PicksTheHighestStaticEquity) {
  Gaddag gaddag = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves l = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  EquityBot bot(gaddag, l);
  MoveGenerator gen(gaddag);
  for (std::uint64_t seed = 1; seed <= 5; ++seed) {
    Game g(seed);
    int turns = 0;
    while (!g.over()) {
      const Rack& rack = g.rack(g.to_move());
      Rack unseen = UnseenTiles(g.board(), rack);
      Move m = bot.Choose(g);
      double chosen = StaticEquity(m, rack, g.bag_size(), unseen, l);
      for (const Move& p : gen.Generate(g.board(), rack)) {
        double e = StaticEquity(p, rack, g.bag_size(), unseen, l);
        EXPECT_GE(chosen, e);
        if (e == chosen) EXPECT_FALSE(TiebreaksBetter(p, m));
      }
      EXPECT_GE(chosen, StaticEquity(Move{}, rack, g.bag_size(), unseen, l));
      g.Apply(m);  // throws on an illegal move
      ASSERT_LT(++turns, 200);
    }
  }
}

}  // namespace scrabble
