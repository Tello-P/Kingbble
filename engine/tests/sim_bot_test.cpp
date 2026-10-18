#include "scrabble/sim_bot.hpp"

#include <gtest/gtest.h>

#include <string>

#include "scrabble/equity.hpp"
#include "scrabble/game.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

class SimBotTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves_ = Leaves::FromFile(kFixtures + "/leaves_small.klv2");

  SimParams Params(int threads) const {
    SimParams p;
    p.candidates = 5;
    p.max_iterations = 24;
    p.min_iterations = 8;
    p.batch = 8;
    p.threads = threads;
    p.seed = 7;
    return p;
  }

  // A mid-game position: a few greedy-ish moves from a fixed seed.
  Game MidGame() {
    Game g(3);
    EquityBot bot(gaddag_, leaves_);
    for (int i = 0; i < 4 && !g.over(); ++i) g.Apply(bot.Choose(g));
    return g;
  }
};

}  // namespace

TEST_F(SimBotTest, ResultsAreReproducibleAndIndependentOfThreads) {
  Game g = MidGame();
  SimBot one(gaddag_, leaves_, Params(1));
  SimBot four(gaddag_, leaves_, Params(4));
  Move a = one.Choose(g);
  Move b = four.Choose(g);
  EXPECT_EQ(a, b);
  ASSERT_EQ(one.last_results().size(), four.last_results().size());
  for (std::size_t i = 0; i < one.last_results().size(); ++i) {
    EXPECT_EQ(one.last_results()[i].move, four.last_results()[i].move);
    EXPECT_DOUBLE_EQ(one.last_results()[i].mean, four.last_results()[i].mean);
    EXPECT_EQ(one.last_results()[i].iterations, four.last_results()[i].iterations);
  }
}

TEST_F(SimBotTest, ChoosesTheBestSurvivingCandidate) {
  Game g = MidGame();
  SimBot bot(gaddag_, leaves_, Params(2));
  Move m = bot.Choose(g);
  const auto& r = bot.last_results();
  ASSERT_FALSE(r.empty());
  EXPECT_LE(r.size(), 5u);
  EXPECT_EQ(m, r.front().move);
  EXPECT_FALSE(r.front().pruned);
  for (const SimResult& s : r) {
    EXPECT_GE(s.iterations, s.pruned ? 8 : 1);
    EXPECT_LE(s.iterations, 24);
    if (!s.pruned) EXPECT_GE(r.front().mean, s.mean);
  }
}

// The bot only sees its own rack: moving tiles between the opponent's rack and
// the bag must not change its decision.
TEST_F(SimBotTest, DoesNotPeekAtTheOpponentRack) {
  Game g = MidGame();
  const int me = g.to_move();
  Rack unseen = UnseenTiles(g.board(), g.rack(me));
  std::string tiles = unseen.ToString();
  std::array<Rack, 2> racks_a, racks_b;
  racks_a[me] = racks_b[me] = g.rack(me);
  Rack bag_a, bag_b;
  for (std::size_t i = 0; i < tiles.size(); ++i) {
    Tile t = *TileFromChar(tiles[i]);
    (i < 7 ? racks_a[1 - me] : bag_a).Add(t);
    (i + 7 >= tiles.size() ? racks_b[1 - me] : bag_b).Add(t);
  }
  std::array<int, 2> scores{g.score(0), g.score(1)};
  Game a = Game::FromPosition(g.board(), racks_a, scores, me, bag_a, 0, 1);
  Game b = Game::FromPosition(g.board(), racks_b, scores, me, bag_b, 0, 2);
  ASSERT_NE(a.rack(1 - me), b.rack(1 - me));
  SimBot bot_a(gaddag_, leaves_, Params(1));
  SimBot bot_b(gaddag_, leaves_, Params(1));
  EXPECT_EQ(bot_a.Choose(a), bot_b.Choose(b));
  EXPECT_DOUBLE_EQ(bot_a.last_results().front().mean, bot_b.last_results().front().mean);
}

TEST_F(SimBotTest, PlaysLegalGamesAgainstEquityBot) {
  SimParams p = Params(2);
  p.max_iterations = 8;
  SimBot sim(gaddag_, leaves_, p);
  EquityBot equity(gaddag_, leaves_);
  for (std::uint64_t seed = 1; seed <= 2; ++seed) {
    Game g(seed);
    int turns = 0;
    while (!g.over()) {
      if (g.bag_size() == 0 && g.to_move() == 0) {
        sim.Choose(g);
        for (const SimResult& s : sim.last_results()) EXPECT_LE(s.iterations, 1);
      }
      g.Apply(g.to_move() == 0 ? sim.Choose(g) : equity.Choose(g));  // throws if illegal
      ASSERT_LT(++turns, 200);
    }
  }
}

TEST(GameFromPosition, KeepsTheGivenState) {
  Board b;
  b.Place(7, 7, *TileFromChar('A'), false);
  std::array<Rack, 2> racks{*Rack::FromString("BCD"), *Rack::FromString("EFG")};
  Rack bag = *Rack::FromString("HIJKLMNOP");
  Game g = Game::FromPosition(b, racks, {10, 20}, 1, bag, 2, 5);
  EXPECT_EQ(g.board().At(7, 7), *TileFromChar('A'));
  EXPECT_EQ(g.rack(0), racks[0]);
  EXPECT_EQ(g.rack(1), racks[1]);
  EXPECT_EQ(g.score(0), 10);
  EXPECT_EQ(g.score(1), 20);
  EXPECT_EQ(g.to_move(), 1);
  EXPECT_EQ(g.bag_size(), 9);
  EXPECT_EQ(g.scoreless_turns(), 2);
  g.Apply(Move{});
  EXPECT_EQ(g.scoreless_turns(), 3);
  EXPECT_EQ(g.to_move(), 0);
}

}  // namespace scrabble
