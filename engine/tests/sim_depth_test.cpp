#include <gtest/gtest.h>

#include <string>

#include "scrabble/equity_bot.hpp"
#include "scrabble/human_reply.hpp"
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

// #56: a time budget that is not reached changes nothing, and a candidate
// clearly ahead of the rest ends the simulation before the budget or the
// iteration cap (the confidence stop: every other candidate is pruned).
TEST_F(SimDepthTest, AnUnreachedTimeLimitChangesNothing) {
  Game g = MidGame();
  SimBot plain(gaddag_, leaves_, Params(2));
  SimParams p = Params(2);
  p.time_limit_s = 1000;
  SimBot timed(gaddag_, leaves_, p);
  EXPECT_EQ(plain.Choose(g), timed.Choose(g));
  ASSERT_EQ(plain.last_results().size(), timed.last_results().size());
  for (std::size_t i = 0; i < plain.last_results().size(); ++i) {
    EXPECT_EQ(plain.last_results()[i].mean, timed.last_results()[i].mean);
    EXPECT_EQ(plain.last_results()[i].iterations, timed.last_results()[i].iterations);
  }
}

TEST_F(SimDepthTest, AClearBestStopsBeforeTheBudget) {
  Game g = MidGame();
  SimParams p = Params(2);
  p.prune_z = 0.5;
  p.max_iterations = 100000;
  p.time_limit_s = 60;
  SimBot bot(gaddag_, leaves_, p);
  bot.Choose(g);
  const auto& r = bot.last_results();
  ASSERT_GT(r.size(), 1u);
  EXPECT_FALSE(r.front().pruned);
  EXPECT_LT(r.front().iterations, p.max_iterations);
  for (std::size_t i = 1; i < r.size(); ++i) EXPECT_TRUE(r[i].pruned);
}

// Sorted means of the candidates, to compare two runs.
std::vector<double> Means(const SimBot& bot) {
  std::vector<double> out;
  for (const SimResult& r : bot.last_results()) out.push_back(r.mean);
  return out;
}

TEST_F(SimDepthTest, RandomRepliesAreReproducibleAcrossThreads) {
  Game g = MidGame();
  SimParams p = Params(3);
  p.prune_z = 1e9;
  SimBot plain(gaddag_, leaves_, p);
  p.reply_top = 3;
  p.reply_temperature = 5;
  SimBot one(gaddag_, leaves_, p);
  p.threads = 3;
  SimBot three(gaddag_, leaves_, p);
  plain.Choose(g);
  EXPECT_EQ(one.Choose(g), three.Choose(g));
  EXPECT_EQ(Means(one), Means(three));
  EXPECT_NE(Means(one), Means(plain));
}

TEST_F(SimDepthTest, HumanRepliesAreLegalAndReproducible) {
  Game g = MidGame();
  HumanModelParams model;
  HumanReplyPolicy policy(gaddag_, leaves_, model, nullptr);
  std::mt19937_64 rng(5);
  for (int i = 0; i < 20; ++i) {
    Game copy = g;
    copy.Apply(policy.Pick(g, rng));  // throws on an illegal move
  }
  SimParams p = Params(3);
  p.prune_z = 1e9;
  auto make = [&] { return std::make_unique<HumanReplyPolicy>(gaddag_, leaves_, model, nullptr); };
  SimBot a(gaddag_, leaves_, p), b(gaddag_, leaves_, p), plain(gaddag_, leaves_, p);
  a.set_reply_policy(make);
  b.set_reply_policy(make);
  plain.Choose(g);
  EXPECT_EQ(a.Choose(g), b.Choose(g));
  EXPECT_EQ(Means(a), Means(b));
  EXPECT_NE(Means(a), Means(plain));
}

TEST_F(SimDepthTest, PlayoutsGoToTheEndWithFewTilesUnseen) {
  // A late position: equity self-play until the bag holds at most 4 tiles.
  Game late(3);
  EquityBot equity(gaddag_, leaves_);
  while (!late.over() && late.bag_size() > 4) late.Apply(equity.Choose(late));
  ASSERT_FALSE(late.over());
  ASSERT_GT(late.bag_size(), 0);
  SimParams p = Params(3);
  p.prune_z = 1e9;
  SimBot three(gaddag_, leaves_, p);
  p.end_unseen = 14;
  SimBot to_end(gaddag_, leaves_, p);
  p.end_unseen = 0;
  p.plies = 1000;
  SimBot deep(gaddag_, leaves_, p);
  three.Choose(late);
  to_end.Choose(late);
  deep.Choose(late);
  EXPECT_EQ(Means(to_end), Means(deep));
  EXPECT_NE(Means(to_end), Means(three));
  // Above the threshold nothing changes.
  Game g = MidGame();
  SimParams q = Params(3);
  q.prune_z = 1e9;
  SimBot plain(gaddag_, leaves_, q);
  q.end_unseen = 14;
  SimBot gated(gaddag_, leaves_, q);
  EXPECT_EQ(plain.Choose(g), gated.Choose(g));
  EXPECT_EQ(Means(plain), Means(gated));
}

TEST_F(SimDepthTest, LateLeaveValuesOnlyActNearTheEnd) {
  Game late(3);
  EquityBot equity(gaddag_, leaves_);
  while (!late.over() && late.bag_size() > 5) late.Apply(equity.Choose(late));
  ASSERT_FALSE(late.over());
  SimParams p = Params(3);
  p.prune_z = 1e9;
  SimBot plain(gaddag_, leaves_, p);
  p.late_bag = 7;
  SimBot blended(gaddag_, leaves_, p);
  plain.Choose(late);
  blended.Choose(late);
  EXPECT_NE(Means(plain), Means(blended));
  Game g = MidGame();  // a full bag: nothing changes
  ASSERT_GT(g.bag_size(), 7 + 7 * 3);
  EXPECT_EQ(plain.Choose(g), blended.Choose(g));
  EXPECT_EQ(Means(plain), Means(blended));
}

TEST_F(SimDepthTest, ReseedingRestartsTheRandomStreams) {
  Game g = MidGame();
  SimParams p = Params(3);
  SimBot used(gaddag_, leaves_, p);
  used.Choose(g);
  used.Choose(g);
  p.seed = 99;
  SimBot fresh(gaddag_, leaves_, p);
  used.Reseed(99);
  EXPECT_EQ(used.Choose(g), fresh.Choose(g));
  EXPECT_EQ(Means(used), Means(fresh));
}

}  // namespace scrabble
