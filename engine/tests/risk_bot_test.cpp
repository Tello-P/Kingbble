#include "scrabble/risk_bot.hpp"

#include <gtest/gtest.h>

#include <string>

#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/inference.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

class RiskBotTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves_ = Leaves::FromFile(kFixtures + "/leaves_small.klv2");

  static SimParams Sim(double prune_z = 1e9) {
    SimParams p;
    p.candidates = 6;
    p.max_iterations = 24;
    p.min_iterations = 8;
    p.batch = 8;
    p.prune_z = prune_z;
    p.seed = 7;
    return p;
  }

  // A position from equity self-play with at most `max_bag` tiles in the bag
  // and the player to move `lead` points ahead (or behind).
  Game Position(int max_bag, int lead) {
    Game g(3);
    EquityBot bot(gaddag_, leaves_);
    while (!g.over() && g.bag_size() > max_bag) g.Apply(bot.Choose(g));
    const int me = g.to_move();
    const Rack bag = MinusTiles(UnseenTiles(g.board(), g.rack(me)), g.rack(1 - me));
    std::array<int, 2> scores{};
    scores[me] = 300 + lead;
    scores[1 - me] = 300;
    return Game::FromPosition(g.board(), {g.rack(0), g.rack(1)}, scores, me, bag,
                              g.scoreless_turns(), 11);
  }
};

SimResult Candidate(std::vector<double> values) {
  SimResult s;
  s.iterations = static_cast<int>(values.size());
  for (double v : values) s.mean += v / values.size();
  s.values = std::move(values);
  return s;
}

}  // namespace

TEST(RiskLambdaTest, FollowsTheExpectedMarginAndTheBag) {
  RiskParams p;
  p.win.rest_sd0 = 0;
  p.win.rest_sd_per_sqrt_tile = 10;
  // sigma^2 = 100 * bag.
  EXPECT_DOUBLE_EQ(RiskLambda(50, 20, p), 50.0 / (2 * 2000));
  EXPECT_DOUBLE_EQ(RiskLambda(-50, 20, p), -50.0 / (2 * 2000));
  EXPECT_DOUBLE_EQ(RiskLambda(0, 20, p), 0);
  EXPECT_GT(RiskLambda(50, 7, p), RiskLambda(50, 20, p));
  EXPECT_DOUBLE_EQ(RiskLambda(50, 0, p), 0);
  p.lambda_scale = 2;
  EXPECT_DOUBLE_EQ(RiskLambda(50, 20, p), 50.0 / 2000);
}

TEST(ScoreRiskTest, ShrinksVariancesTowardTheirMean) {
  // Own variances 0 and 8 (values -2, 2, ... around 0); pool 4.
  std::vector<SimResult> sims = {Candidate({1, 1, 1, 1}), Candidate({-2, 2, -2, 2})};
  Game g(1);
  RiskParams p;
  p.min_bag = 0;
  p.max_bag = 100;
  p.decided = 0;
  p.shrink = 0;
  std::vector<RiskResult> r = ScoreRisk(g, sims, p);
  EXPECT_NEAR(r[0].variance + r[1].variance, 0 + 16.0 / 3, 1e-12);
  p.shrink = 4;  // halfway to the pool
  r = ScoreRisk(g, sims, p);
  double lo = std::min(r[0].variance, r[1].variance), hi = std::max(r[0].variance, r[1].variance);
  EXPECT_NEAR(lo, (0 + 8.0 / 3) / 2, 1e-12);
  EXPECT_NEAR(hi, (16.0 / 3 + 8.0 / 3) / 2, 1e-12);
}

TEST(ScoreRiskTest, RelativeVarianceRemovesTheCommonNoise) {
  // Both candidates swing with the same draws; the second adds a constant.
  std::vector<SimResult> sims = {Candidate({-20, 20, -20, 20}), Candidate({-19, 21, -19, 21})};
  Game g(1);
  RiskParams p;
  p.min_bag = 0;
  p.max_bag = 100;
  p.decided = 0;
  p.shrink = 0;
  p.relative_variance = true;
  for (const RiskResult& r : ScoreRisk(g, sims, p)) EXPECT_DOUBLE_EQ(r.variance, 0);
  p.relative_variance = false;
  for (const RiskResult& r : ScoreRisk(g, sims, p)) EXPECT_GT(r.variance, 500);
}

TEST_F(RiskBotTest, OutsideTheWindowItPlaysLikeSimBot) {
  Game g = Position(60, -40);
  ASSERT_GT(g.bag_size(), 25);
  RiskParams risk;
  risk.lambda_scale = 1e6;
  RiskBot risky(gaddag_, leaves_, Sim(2.58), risk);
  SimBot sim(gaddag_, leaves_, Sim(2.58));
  EXPECT_EQ(risky.Choose(g), sim.Choose(g));
  EXPECT_FALSE(risky.last_active());
}

TEST_F(RiskBotTest, WithoutRiskItPlaysLikeSimBot) {
  Game g = Position(20, -40);
  RiskParams risk;
  risk.lambda_scale = 0;
  RiskBot risky(gaddag_, leaves_, Sim(), risk);
  SimBot sim(gaddag_, leaves_, Sim());
  EXPECT_EQ(risky.Choose(g), sim.Choose(g));
}

TEST_F(RiskBotTest, LeadingPicksLowVarianceAndTrailingHighVariance) {
  RiskParams risk;
  risk.lambda_scale = 1e6;  // variance dominates the mean
  risk.decided = 0;
  for (int lead : {150, -150}) {
    Game g = Position(20, lead);
    ASSERT_TRUE(InRiskWindow(g, risk));
    RiskBot bot(gaddag_, leaves_, Sim(), risk);
    Move m = bot.Choose(g);
    EXPECT_TRUE(bot.last_active());
    const auto& r = bot.last_results();
    ASSERT_GT(r.size(), 1u);
    EXPECT_EQ(m, r.front().sim.move);
    // Against SimBot's choice (best mean): less variance ahead, more behind.
    const RiskResult* by_mean = &r.front();
    for (const RiskResult& x : r) {
      if (x.sim.mean > by_mean->sim.mean) by_mean = &x;
      EXPECT_EQ(x.lambda > 0, lead > 0);
    }
    if (lead > 0) EXPECT_LE(r.front().variance, by_mean->variance);
    if (lead < 0) EXPECT_GE(r.front().variance, by_mean->variance);
  }
}

TEST_F(RiskBotTest, ADecidedGameIsLeftToSimBot) {
  RiskParams risk;
  risk.lambda_scale = 1e6;
  Game g = Position(20, 400);
  RiskBot bot(gaddag_, leaves_, Sim(), risk);
  SimBot sim(gaddag_, leaves_, Sim());
  EXPECT_EQ(bot.Choose(g), sim.Choose(g));
  EXPECT_FALSE(bot.last_active());
}

TEST_F(RiskBotTest, WinModePicksTheHighestWinProbability) {
  RiskParams risk;
  risk.mode = RiskMode::kWinProb;
  risk.decided = 0;
  Game g = Position(20, -30);
  RiskBot bot(gaddag_, leaves_, Sim(), risk);
  Move m = bot.Choose(g);
  const auto& r = bot.last_results();
  ASSERT_FALSE(r.empty());
  EXPECT_EQ(m, r.front().sim.move);
  for (const RiskResult& x : r) {
    EXPECT_DOUBLE_EQ(x.utility, x.win.win);
    EXPECT_GE(r.front().win.win, x.win.win);
    EXPECT_GE(x.variance, 0);
  }
}

TEST_F(RiskBotTest, MoreIterationsInsideTheWindow) {
  RiskParams risk;
  risk.iterations_factor = 2;
  risk.decided = 0;
  Game g = Position(20, 0);
  RiskBot bot(gaddag_, leaves_, Sim(), risk);
  bot.Choose(g);
  for (const RiskResult& x : bot.last_results()) EXPECT_EQ(x.sim.iterations, 48);
}

}  // namespace scrabble
