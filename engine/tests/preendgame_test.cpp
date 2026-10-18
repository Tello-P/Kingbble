#include "scrabble/preendgame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>

#include "scrabble/endgame.hpp"
#include "scrabble/endgame_bot.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/preendgame_bot.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

class PreEndgameTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves_ = Leaves::FromFile(kFixtures + "/leaves_small.klv2");

  // A small pre-endgame: the first position of a static-equity game with an
  // empty bag and at most `max_tiles` tiles on both racks, `bag` of the
  // opponent's tiles put back in the bag.
  Game PreEndgame(std::uint64_t seed, int bag, int max_tiles = 8) {
    EquityBot bot(gaddag_, leaves_);
    for (;; ++seed) {
      Game g(seed);
      while (!g.over() && (g.bag_size() > 0 || g.rack(0).Size() + g.rack(1).Size() > max_tiles))
        g.Apply(bot.Choose(g));
      const int me = g.to_move();
      if (g.over() || g.rack(1 - me).Size() <= bag || g.rack(me).Size() < 2) continue;
      std::array<Rack, 2> racks = {g.rack(0), g.rack(1)};
      Rack in_bag;
      std::string opp = racks[1 - me].ToString();
      for (int i = 0; i < bag; ++i) {
        in_bag.Add(*TileFromChar(opp[i]));
        racks[1 - me].Remove(*TileFromChar(opp[i]));
      }
      return Game::FromPosition(g.board(), racks, {g.score(0), g.score(1)}, me, in_bag, 0, 1);
    }
  }

  // The same position with the unseen tiles split differently between the
  // opponent's rack and the bag.
  static Game Resplit(const Game& g, bool reverse) {
    const int me = g.to_move();
    std::string tiles = UnseenTiles(g.board(), g.rack(me)).ToString();
    if (reverse) std::reverse(tiles.begin(), tiles.end());
    std::array<Rack, 2> racks;
    racks[me] = g.rack(me);
    Rack bag;
    for (std::size_t i = 0; i < tiles.size(); ++i)
      (static_cast<int>(i) < g.bag_size() ? bag : racks[1 - me]).Add(*TileFromChar(tiles[i]));
    return Game::FromPosition(g.board(), racks, {g.score(0), g.score(1)}, me, bag,
                              g.scoreless_turns(), 7);
  }

  // Brute force for one tile in the bag: whether `move` wins (1), ties (0.5) or
  // loses (0) when `bag` is the tile left, the opponent answering with every
  // reply and both endgames solved exactly. Two passes in a row end the game.
  double Outcome(const Game& g, const Move& move, Tile bag) {
    const int me = g.to_move();
    Rack opp = UnseenTiles(g.board(), g.rack(me));
    opp.Remove(bag);
    Rack in_bag;
    in_bag.Add(bag);
    std::array<Rack, 2> racks;
    racks[me] = g.rack(me);
    racks[1 - me] = opp;
    Game next = Game::FromPosition(g.board(), racks, {g.score(0), g.score(1)}, me, in_bag, 0, 1);
    next.Apply(move);
    auto sign = [](int spread) { return spread > 0 ? 1.0 : spread == 0 ? 0.5 : 0.0; };
    EndgameSolver solver(gaddag_);
    auto spread = [&](const Game& x) { return x.score(me) - x.score(1 - me); };
    auto solved = [&](const Game& x) {
      if (x.over()) return sign(spread(x));
      const int v = solver.Solve(x).value;
      return sign(spread(x) + (x.to_move() == me ? v : -v));
    };
    if (next.bag_size() == 0) return solved(next);
    // We passed: the opponent picks the reply that is worst for us.
    MoveGenerator gen(gaddag_);
    std::vector<Move> replies = gen.Generate(next.board(), next.rack(1 - me));
    double worst = 1;
    for (const Move& r : replies) {
      Game after = next;
      after.Apply(r);
      worst = std::min(worst, solved(after));
    }
    const int penalty = RackValue(next.rack(me)) - RackValue(next.rack(1 - me));
    return std::min(worst, sign(spread(next) - penalty));  // passing back ends the game
  }
};

double Permutations(int n, int k) {
  double r = 1;
  for (int i = 0; i < k; ++i) r *= n - i;
  return r;
}

}  // namespace

TEST(PreEndgamePermutations, CountEveryOrderedDraw) {
  Rack pool = *Rack::FromString("?AAEEQSTT");
  for (int k = 1; k <= 3; ++k) {
    auto draws = PreEndgameSolver::Permutations(pool, k);
    double total = 0;
    std::set<std::string> seen;
    for (const auto& d : draws) {
      ASSERT_EQ(d.size, k);
      EXPECT_TRUE(seen.insert(std::string(d.tiles.begin(), d.tiles.begin() + k)).second);
      total += d.count;
    }
    EXPECT_DOUBLE_EQ(total, Permutations(9, k));
  }
  // Ordered pairs of 6 letter kinds, minus the four that need two Qs, ? or more.
  EXPECT_EQ(PreEndgameSolver::Permutations(pool, 2).size(), 6u * 6u - 3u);
}

TEST_F(PreEndgameTest, MatchesBruteForceWithOneTileInTheBag) {
  int placements = 0, passes = 0;
  for (std::uint64_t seed : {1, 5, 9}) {
    Game g = PreEndgame(seed, 1);
    ASSERT_EQ(g.bag_size(), 1);
    PreEndgameParams params;
    params.candidates = 3;
    params.pass_candidate = true;
    params.early_cutoff = false;
    PreEndgameSolver solver(gaddag_, leaves_);
    PreEndgameResult r = solver.Solve(g, params);
    const Rack unseen = UnseenTiles(g.board(), g.rack(g.to_move()));
    EXPECT_DOUBLE_EQ(r.permutations, unseen.Size());
    for (const PreEndgamePlay& p : r.plays) {
      if (p.skipped) continue;
      ++(p.move.kind == Move::Kind::kPass ? passes : placements);
      double wins = 0;
      for (int t = 0; t < kNumTileTypes; ++t)
        if (unseen.Count(Tile(t))) wins += unseen.Count(Tile(t)) * Outcome(g, p.move, Tile(t));
      EXPECT_DOUBLE_EQ(p.wins, wins) << p.move.ToString();
    }
  }
  EXPECT_EQ(passes, 3);
  EXPECT_GE(placements, 4);
}

TEST_F(PreEndgameTest, RanksByWinsThenTiebreakSpread) {
  Game g = PreEndgame(2, 1);
  PreEndgameParams params;
  params.candidates = 6;
  PreEndgameSolver solver(gaddag_, leaves_);
  PreEndgameResult r = solver.Solve(g, params);
  ASSERT_TRUE(r.complete);
  const PreEndgamePlay& best = r.plays.front();
  for (const PreEndgamePlay& p : r.plays) {
    if (p.skipped || p.cut) continue;
    EXPECT_LE(p.wins, best.wins + 1e-9);
    if (p.has_spread) EXPECT_LE(p.spread, best.spread);
  }
}

TEST_F(PreEndgameTest, DoesNotPeekAtTheOpponentRack) {
  Game g = PreEndgame(3, 2);
  PreEndgameParams params;
  params.candidates = 4;
  PreEndgameSolver solver(gaddag_, leaves_);
  PreEndgameResult a = solver.Solve(Resplit(g, false), params);
  PreEndgameResult b = solver.Solve(Resplit(g, true), params);
  ASSERT_EQ(a.plays.size(), b.plays.size());
  for (std::size_t i = 0; i < a.plays.size(); ++i) {
    EXPECT_EQ(a.plays[i].move, b.plays[i].move);
    EXPECT_DOUBLE_EQ(a.plays[i].wins, b.plays[i].wins);
  }
}

TEST_F(PreEndgameTest, SolvesNestedPreEndgames) {
  // Two tiles in the bag: one-tile plays leave one, and after the opponent's
  // reply it is our turn again with a tile to draw.
  Game g = PreEndgame(4, 2);
  ASSERT_EQ(g.bag_size(), 2);
  PreEndgameParams params;
  params.candidates = 4;
  params.early_cutoff = false;
  PreEndgameSolver solver(gaddag_, leaves_);
  PreEndgameResult r = solver.Solve(g, params);
  ASSERT_TRUE(r.complete);
  const double total = r.permutations;
  EXPECT_DOUBLE_EQ(total, Permutations(UnseenTiles(g.board(), g.rack(g.to_move())).Size(), 2));
  for (const PreEndgamePlay& p : r.plays) {
    if (p.skipped) continue;
    EXPECT_GE(p.wins, 0);
    EXPECT_LE(p.wins + p.losses, total + 1e-9);
  }
}

TEST_F(PreEndgameTest, StopsAtTheTimeLimit) {
  Game g = PreEndgame(6, 2, 14);
  PreEndgameParams params;
  params.time_limit_s = 1e-6;
  PreEndgameSolver solver(gaddag_, leaves_);
  EXPECT_FALSE(solver.Solve(g, params).complete);
}

TEST_F(PreEndgameTest, BotDefersToTheInnerBotWithAFullBag) {
  Game g(1);
  EquityBot equity(gaddag_, leaves_);
  PreEndgameBot bot(std::make_unique<EquityBot>(gaddag_, leaves_), gaddag_, leaves_, 1, {});
  EXPECT_EQ(bot.Choose(g), equity.Choose(g));
  EXPECT_TRUE(bot.last_result().plays.empty());
}

TEST_F(PreEndgameTest, BotPlaysLegalGames) {
  PreEndgameParams params;
  params.candidates = 3;
  params.time_limit_s = 0.5;
  EndgameBot bot(std::make_unique<PreEndgameBot>(std::make_unique<EquityBot>(gaddag_, leaves_),
                                                 gaddag_, leaves_, 2, params),
                 gaddag_, 0.5);
  for (std::uint64_t seed = 1; seed <= 3; ++seed) {
    Game g(seed);
    int turns = 0;
    while (!g.over()) {
      g.Apply(bot.Choose(g));
      ASSERT_LT(++turns, 200);
    }
  }
}

}  // namespace scrabble
