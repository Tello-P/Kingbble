#include "scrabble/approx_preendgame.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "scrabble/approx_preendgame_bot.hpp"
#include "scrabble/endgame_bot.hpp"
#include "scrabble/equity_bot.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

class ApproxPreEndgameTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves_ = Leaves::FromFile(kFixtures + "/leaves_small.klv2");

  // Position of a static-equity game once the bag holds at most `bag` tiles.
  Game Position(std::uint64_t seed, int bag) {
    EquityBot bot(gaddag_, leaves_);
    for (;; ++seed) {
      Game g(seed);
      while (!g.over() && g.bag_size() > bag) g.Apply(bot.Choose(g));
      if (!g.over() && g.bag_size() > 0) return g;
    }
  }
};

}  // namespace

TEST_F(ApproxPreEndgameTest, RanksTheBestStaticPlays) {
  Game g = Position(1, 4);
  ApproxPreEndgameParams params;
  params.candidates = 5;
  params.samples = 12;
  params.endgame_plies = 2;
  ApproxPreEndgameSolver solver(gaddag_, leaves_);
  auto plays = solver.Solve(g, params);
  ASSERT_EQ(plays.size(), 5u);
  for (std::size_t i = 0; i < plays.size(); ++i) {
    EXPECT_EQ(plays[i].samples, 12);
    EXPECT_GE(plays[i].wins, 0);
    EXPECT_LE(plays[i].wins, 1);
    if (i > 0) {
      EXPECT_TRUE(plays[i - 1].wins > plays[i].wins ||
                  (plays[i - 1].wins == plays[i].wins && plays[i - 1].spread >= plays[i].spread));
    }
    Game next = g;
    next.Apply(plays[i].move);  // throws if illegal
  }
}

TEST_F(ApproxPreEndgameTest, ThreadsGiveTheSameResults) {
  Game g = Position(2, 3);
  ApproxPreEndgameParams params;
  params.candidates = 4;
  params.samples = 10;
  params.endgame_plies = 2;
  ApproxPreEndgameSolver solver(gaddag_, leaves_);
  auto one = solver.Solve(g, params);
  params.threads = 3;
  auto three = solver.Solve(g, params);
  ASSERT_EQ(one.size(), three.size());
  for (std::size_t i = 0; i < one.size(); ++i) {
    EXPECT_EQ(one[i].move, three[i].move);
    EXPECT_DOUBLE_EQ(one[i].wins, three[i].wins);
    EXPECT_DOUBLE_EQ(one[i].spread, three[i].spread);
  }
}

TEST_F(ApproxPreEndgameTest, EveryPlaySeesTheSameSamplesUnderATimeLimit) {
  Game g = Position(3, 5);
  ApproxPreEndgameParams params;
  params.samples = 100000;
  params.time_limit_s = 0.2;
  params.threads = 2;
  ApproxPreEndgameSolver solver(gaddag_, leaves_);
  auto plays = solver.Solve(g, params);
  ASSERT_FALSE(plays.empty());
  EXPECT_LT(plays[0].samples, 100000);
  for (const auto& p : plays) EXPECT_EQ(p.samples, plays[0].samples);
}

TEST_F(ApproxPreEndgameTest, BotPlaysLegalGames) {
  ApproxPreEndgameParams params;
  params.candidates = 3;
  params.samples = 4;
  params.endgame_plies = 2;
  EndgameBot bot(std::make_unique<ApproxPreEndgameBot>(
                     std::make_unique<EquityBot>(gaddag_, leaves_), gaddag_, leaves_, 1, 6, params),
                 gaddag_, 0.2);
  for (std::uint64_t seed = 1; seed <= 2; ++seed) {
    Game g(seed);
    int turns = 0;
    while (!g.over()) {
      g.Apply(bot.Choose(g));
      ASSERT_LT(++turns, 200);
    }
  }
}

}  // namespace scrabble
