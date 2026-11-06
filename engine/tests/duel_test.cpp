#include "duel.hpp"

#include <gtest/gtest.h>

#include <string>

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

class DuelTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves_ = Leaves::FromFile(kFixtures + "/leaves_small.klv2");

  Game MidGame() {
    Game g(3);
    EquityBot bot(gaddag_, leaves_);
    for (int i = 0; i < 6 && !g.over(); ++i) g.Apply(bot.Choose(g));
    return g;
  }
};

TEST_F(DuelTest, AMoveAgainstItselfIsATie) {
  const Game game = MidGame();
  EquityBot bot(gaddag_, leaves_);
  const Move best = bot.Choose(game);
  const DuelResult r = Duel(game, best, best, gaddag_, leaves_, 20, 2, 5);
  EXPECT_EQ(r.win, 0);
  EXPECT_EQ(r.spread, 0);
  EXPECT_EQ(r.rollouts, 20);
}

TEST_F(DuelTest, ThrowingAwayATurnLosesPoints) {
  const Game game = MidGame();
  EquityBot bot(gaddag_, leaves_);
  const Move best = bot.Choose(game);
  ASSERT_GT(best.score, 0);
  const DuelResult r = Duel(game, best, Move{}, gaddag_, leaves_, 40, 2, 5);  // a pass
  EXPECT_GT(r.spread, 0);
}

TEST_F(DuelTest, ResultsDoNotDependOnThreads) {
  const Game game = MidGame();
  EquityBot bot(gaddag_, leaves_);
  const Move best = bot.Choose(game);
  // No endgame solves: their time limit would depend on the machine.
  const DuelResult one = Duel(game, best, Move{}, gaddag_, leaves_, 16, 1, 9, 0);
  const DuelResult two = Duel(game, best, Move{}, gaddag_, leaves_, 16, 2, 9, 0);
  EXPECT_DOUBLE_EQ(one.spread, two.spread);
  EXPECT_DOUBLE_EQ(one.win, two.win);
}

}  // namespace

}  // namespace scrabble
