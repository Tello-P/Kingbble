#include "scrabble/win_prob.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <string>

#include "scrabble/equity_bot.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

}  // namespace

TEST(WinProbTest, ExactMarginsCountTiesAsHalf) {
  EXPECT_DOUBLE_EQ(EstimateWin({}, 5, 0).win, 1.0);
  EXPECT_DOUBLE_EQ(EstimateWin({}, -5, 0).win, 0.0);
  EXPECT_DOUBLE_EQ(EstimateWin({}, 0, 0).win, 0.5);

  WinEstimate est = EstimateWin({-10, 0, 10, 20}, 0, 0);
  EXPECT_DOUBLE_EQ(est.win, 0.625);
  // p = {0, 0.5, 1, 1}: sample variance 0.229166..., over n = 4.
  EXPECT_NEAR(est.std_error, std::sqrt(0.6875 / 3 / 4), 1e-12);
  EXPECT_DOUBLE_EQ(EstimateWin({-10, 0, 10, 20}, 10, 0).win, 0.875);
}

TEST(WinProbTest, NormalNoiseFollowsTheMargin) {
  EXPECT_NEAR(EstimateWin({0}, 0, 10).win, 0.5, 1e-12);
  EXPECT_NEAR(EstimateWin({0}, 10, 10).win, 0.841344746, 1e-8);
  EXPECT_NEAR(EstimateWin({-30, 10}, 20, 10).win, (0.158655254 + 0.998650102) / 2, 1e-8);
  // More noise to come pulls a leader towards even.
  EXPECT_GT(EstimateWin({15}, 0, 10).win, EstimateWin({15}, 0, 40).win);
  EXPECT_GT(EstimateWin({15}, 0, 40).win, 0.5);
  // A riskier spread with the same mean helps a trailing player.
  EXPECT_GT(EstimateWin({-60, 40}, 0, 5).win, EstimateWin({-10, -10}, 0, 5).win);
}

TEST(WinProbTest, RestSpreadGrowsWithTheBag) {
  WinParams params;
  EXPECT_DOUBLE_EQ(RestSpreadSd(0, params), 0.0);
  EXPECT_DOUBLE_EQ(RestSpreadSd(-3, params), 0.0);
  EXPECT_DOUBLE_EQ(RestSpreadSd(16, params), 40.0);
  params.spread_sd_per_tile = 5;
  EXPECT_DOUBLE_EQ(RestSpreadSd(16, params), 20.0);
}

TEST(WinProbTest, EstimatesEverySimulatedCandidate) {
  Gaddag gaddag = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  Game g(3);
  EquityBot equity(gaddag, leaves);
  for (int i = 0; i < 4 && !g.over(); ++i) g.Apply(equity.Choose(g));

  SimParams p;
  p.candidates = 5;
  p.max_iterations = 24;
  p.min_iterations = 8;
  p.batch = 8;
  p.seed = 7;
  SimBot bot(gaddag, leaves, p);
  bot.Choose(g);
  const auto& results = bot.last_results();
  std::vector<WinEstimate> wins = WinProbabilities(g, results, WinParams{});
  ASSERT_EQ(wins.size(), results.size());
  for (std::size_t i = 0; i < results.size(); ++i) {
    const SimResult& r = results[i];
    ASSERT_EQ(r.values.size(), static_cast<std::size_t>(r.iterations));
    EXPECT_NEAR(std::accumulate(r.values.begin(), r.values.end(), 0.0) / r.iterations, r.mean,
                1e-9);
    EXPECT_GE(wins[i].win, 0.0);
    EXPECT_LE(wins[i].win, 1.0);
    EXPECT_GE(wins[i].std_error, 0.0);
  }
}

}  // namespace scrabble
