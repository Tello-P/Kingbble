#include <gtest/gtest.h>

#include <string>

#include "scrabble/equity_bot.hpp"
#include "scrabble/sim_bot.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

class SimDepthTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves_ = Leaves::FromFile(kFixtures + "/leaves_small.klv2");

  static SimParams Params(int plies) {
    SimParams p;
    p.candidates = 5;
    p.max_iterations = 24;
    p.min_iterations = 8;
    p.batch = 8;
    p.seed = 7;
    p.plies = plies;
    return p;
  }

  Game MidGame() {
    Game g(3);
    EquityBot bot(gaddag_, leaves_);
    for (int i = 0; i < 4 && !g.over(); ++i) g.Apply(bot.Choose(g));
    return g;
  }
};

}  // namespace

TEST_F(SimDepthTest, DeeperPlayoutsChangeTheValuesAndStayReproducible) {
  Game g = MidGame();
  SimBot two(gaddag_, leaves_, Params(2));
  SimBot four(gaddag_, leaves_, Params(4));
  SimBot four_again(gaddag_, leaves_, Params(4));
  two.Choose(g);
  EXPECT_EQ(four.Choose(g), four_again.Choose(g));
  bool differs = false;
  for (const SimResult& a : two.last_results())
    for (const SimResult& b : four.last_results())
      if (a.move == b.move && a.mean != b.mean) differs = true;
  EXPECT_TRUE(differs);
  for (std::size_t i = 0; i < four.last_results().size(); ++i)
    EXPECT_DOUBLE_EQ(four.last_results()[i].mean, four_again.last_results()[i].mean);
}

TEST_F(SimDepthTest, ThreeAndFourPliesKeepTheirIterations) {
  Game g = MidGame();
  for (int plies : {1, 3, 4}) {
    SimBot bot(gaddag_, leaves_, Params(plies));
    Move m = bot.Choose(g);
    ASSERT_FALSE(bot.last_results().empty());
    EXPECT_EQ(m, bot.last_results().front().move);
    for (const SimResult& r : bot.last_results())
      EXPECT_EQ(r.values.size(), static_cast<std::size_t>(r.iterations));
  }
}

TEST_F(SimDepthTest, ATimeLimitStopsAfterTheMinimumIterations) {
  Game g = MidGame();
  SimParams p = Params(2);
  p.prune_z = 1e9;
  p.max_iterations = 100000;
  p.time_limit_s = 1e-9;
  SimBot bot(gaddag_, leaves_, p);
  bot.Choose(g);
  for (const SimResult& r : bot.last_results()) EXPECT_EQ(r.iterations, 8);
}

}  // namespace scrabble
