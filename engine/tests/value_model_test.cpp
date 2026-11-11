#include "scrabble/value_model.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string>

#include "scrabble/equity_bot.hpp"
#include "scrabble/sim_bot.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

std::string Header() { return "kingbble-value 1 " + std::to_string(kValueFeatures) + "\n"; }

double Logistic(double x) { return 1 / (1 + std::exp(-x)); }

TEST(ValueModel, FollowsTheTreesAndAddsTheirLeaves) {
  // Tree 1 splits on the spread (feature 0) at 10; tree 2 is a single leaf.
  const ValueModel model = ValueModel::FromText(Header() +
                                                "tree 1 2\n0 10 -1 -2\n-0.5 0.5\n"
                                                "tree 0 1\n0.25\n");
  std::array<double, kValueFeatures> f{};
  f[0] = 5;
  EXPECT_NEAR(model.Predict(f), Logistic(-0.25), 1e-12);
  f[0] = 11;
  EXPECT_NEAR(model.Predict(f), Logistic(0.75), 1e-12);
}

TEST(ValueModel, RefusesOtherFormats) {
  EXPECT_THROW(ValueModel::FromText("kingbble-value 1 3\n"), std::runtime_error);
  EXPECT_THROW(ValueModel::FromText(Header() + "tree 1 2\n99 0 -1 -2\n0 0\n"), std::runtime_error);
  EXPECT_THROW(ValueModel::FromText(Header() + "tree 1 2\n0 0 -1\n"), std::runtime_error);
}

TEST(ValueFeatures, DescribeTheOpening) {
  Leaves leaves = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  const Game game(3);
  const auto f = ValueFeatures(game, leaves);
  EXPECT_EQ(f[0], 0);   // spread
  EXPECT_EQ(f[1], 93);  // unseen: 100 tiles less our 7
  EXPECT_EQ(f[2], 86);  // bag
  EXPECT_EQ(f[4], 7);   // rack size
  EXPECT_EQ(f[13], 0);  // tiles on the board
  EXPECT_EQ(f[14], 0);  // no open triple word square on an empty board
}

TEST(ValueModel, DrivesTheSimulationAsAChanceOfWinning) {
  Gaddag gaddag = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  const ValueModel model = ValueModel::FromText(Header() + "tree 1 2\n0 0 -1 -2\n-1 1\n");
  Game game(3);
  EquityBot bot(gaddag, leaves);
  for (int i = 0; i < 4; ++i) game.Apply(bot.Choose(game));
  SimParams p;
  p.candidates = 4;
  p.max_iterations = 16;
  p.min_iterations = 8;
  p.batch = 8;
  p.value_model = &model;
  SimBot sim(gaddag, leaves, p);
  sim.Choose(game);
  for (const SimResult& r : sim.last_results()) {
    EXPECT_GE(r.mean, 0);
    EXPECT_LE(r.mean, 1);
  }
}

}  // namespace

}  // namespace scrabble
