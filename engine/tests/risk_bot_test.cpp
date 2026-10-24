#include "scrabble/risk_bot.hpp"

#include <gtest/gtest.h>

#include <string>

#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

class RiskBotTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves_ = Leaves::FromFile(kFixtures + "/leaves_small.klv2");

  static SimParams Sim() {
    SimParams p;
    p.candidates = 6;
    p.max_iterations = 24;
    p.min_iterations = 8;
    p.batch = 8;
    p.prune_z = 1e9;  // keep every candidate
    p.seed = 7;
    return p;
  }

  // The mid-game position of SimBotTest with the player to move `lead` points
  // ahead (or behind).
  Game MidGame(int lead) {
    Game g(3);
    EquityBot bot(gaddag_, leaves_);
    for (int i = 0; i < 4 && !g.over(); ++i) g.Apply(bot.Choose(g));
    const int me = g.to_move();
    const Rack bag = MinusTiles(UnseenTiles(g.board(), g.rack(me)), g.rack(1 - me));
    std::array<int, 2> scores{};
    scores[me] = 200 + lead;
    scores[1 - me] = 200;
    return Game::FromPosition(g.board(), {g.rack(0), g.rack(1)}, scores, me, bag,
                              g.scoreless_turns(), 11);
  }
};

}  // namespace

TEST(RiskLambdaTest, LeadersAvoidRiskAndTrailersSeekIt) {
  RiskParams p;
  EXPECT_GT(RiskLambda(50, 40, p), 0);
  EXPECT_LT(RiskLambda(-50, 40, p), 0);
  EXPECT_DOUBLE_EQ(RiskLambda(0, 40, p), 0);
  // Saturates with the lead and grows as the bag empties.
  EXPECT_DOUBLE_EQ(RiskLambda(300, 0, p), p.max_lambda);
  EXPECT_DOUBLE_EQ(RiskLambda(-300, 0, p), -p.max_lambda);
  EXPECT_DOUBLE_EQ(RiskLambda(100, 86, p), 0);
  EXPECT_GT(RiskLambda(50, 10, p), RiskLambda(50, 60, p));
}

TEST_F(RiskBotTest, WithoutRiskItPlaysLikeSimBot) {
  Game g = MidGame(-40);
  RiskParams risk;
  risk.max_lambda = 0;
  RiskBot risky(gaddag_, leaves_, Sim(), risk);
  SimBot sim(gaddag_, leaves_, Sim());
  EXPECT_EQ(risky.Choose(g), sim.Choose(g));
}

TEST_F(RiskBotTest, LeadingPicksLowVarianceAndTrailingHighVariance) {
  RiskParams risk;
  risk.max_lambda = 1e6;  // variance dominates the mean
  for (int lead : {150, -150}) {
    Game g = MidGame(lead);
    RiskBot bot(gaddag_, leaves_, Sim(), risk);
    Move m = bot.Choose(g);
    const auto& r = bot.last_results();
    ASSERT_GT(r.size(), 1u);
    EXPECT_EQ(m, r.front().sim.move);
    for (const RiskResult& x : r) {
      if (lead > 0) EXPECT_LE(r.front().variance, x.variance);
      if (lead < 0) EXPECT_GE(r.front().variance, x.variance);
    }
  }
}

TEST_F(RiskBotTest, WinModePicksTheHighestWinProbability) {
  RiskParams risk;
  risk.mode = RiskMode::kWinProb;
  Game g = MidGame(-30);
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

}  // namespace scrabble
