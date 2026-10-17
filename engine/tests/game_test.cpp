#include "scrabble/game.hpp"

#include <gtest/gtest.h>

#include "scrabble/greedy_bot.hpp"

namespace scrabble {

namespace {

int TilesInPlay(const Game& g) {
  int n = g.bag_size() + g.rack(0).Size() + g.rack(1).Size();
  for (int r = 0; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c) n += g.board().IsEmpty(r, c) ? 0 : 1;
  return n;
}

}  // namespace

TEST(Game, DealsTwoRacks) {
  Game g(1);
  EXPECT_EQ(g.rack(0).Size(), kRackSize);
  EXPECT_EQ(g.rack(1).Size(), kRackSize);
  EXPECT_EQ(g.bag_size(), 86);
}

TEST(Game, PassesEndTheGameAfterSixScorelessTurns) {
  Game g(1);
  int r0 = RackValue(g.rack(0)), r1 = RackValue(g.rack(1));
  for (int i = 0; i < Game::kMaxScorelessTurns; ++i) g.Apply(Move{});
  EXPECT_TRUE(g.over());
  EXPECT_EQ(g.score(0), -r0);
  EXPECT_EQ(g.score(1), -r1);
}

TEST(Game, GreedySelfPlayConservesTilesAndTerminates) {
  Gaddag gaddag = Gaddag::FromFile(std::string(KINGBBLE_FIXTURES) + "/enable_small.txt");
  GreedyBot bot(gaddag);
  for (std::uint64_t seed = 1; seed <= 5; ++seed) {
    Game g(seed);
    int turns = 0;
    while (!g.over()) {
      int before = g.score(g.to_move());
      Move m = bot.Choose(g);
      int player = g.to_move();
      g.Apply(m);
      if (!g.over() && m.kind == Move::Kind::kPlace) EXPECT_EQ(g.score(player) - before, m.score);
      EXPECT_EQ(TilesInPlay(g), TotalTiles());
      ASSERT_LT(++turns, 200);
    }
  }
}

}  // namespace scrabble
