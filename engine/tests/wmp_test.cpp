#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include "scrabble/anagram_index.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/game.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/movegen.hpp"

namespace scrabble {
namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

std::vector<std::string> Texts(const std::vector<Move>& moves) {
  std::vector<std::string> out;
  for (const Move& m : moves) out.push_back(m.ToString());
  std::sort(out.begin(), out.end());
  return out;
}

double Equity(const void* context, int score, const Rack& leave) {
  return score + static_cast<const Leaves*>(context)->Value(leave);
}

// Self-play positions, each with the mover's rack and random racks (blanks too).
TEST(Wmp, GeneratesExactlyTheSameMoves) {
  Gaddag gaddag = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  AnagramIndex index = AnagramIndex::FromGaddag(gaddag);
  ASSERT_GT(index.NumWords(), 0u);
  MoveGenerator walk(gaddag), wmp(gaddag);
  wmp.set_anagram_index(&index);
  EquityBot bot(gaddag, leaves);
  std::mt19937_64 rng(5);
  const std::string pool = "AAAAABCDEEEEEFGHIIIIJKLMNNOOOOPQRRSSTTUUVWXYZ??";
  long positions = 0, moves = 0;
  for (int seed = 1; seed <= 6; ++seed) {
    Game g(seed);
    for (int turn = 0; turn < 30 && !g.over(); ++turn) {
      std::vector<Rack> racks{g.rack(g.to_move())};
      for (int extra = 0; extra < 3; ++extra) {
        // Seven tiles of the pool, without replacement (so at most two blanks).
        std::string tiles = pool;
        std::shuffle(tiles.begin(), tiles.end(), rng);
        Rack r;
        for (int t = 0; t < kRackSize; ++t)
          r.Add(tiles[t] == '?' ? kBlank : *TileFromChar(tiles[t]));
        racks.push_back(r);
      }
      for (const Rack& rack : racks) {
        const std::vector<Move> a = walk.Generate(g.board(), rack);
        const std::vector<Move> b = wmp.Generate(g.board(), rack);
        ASSERT_EQ(Texts(a), Texts(b)) << "seed " << seed << " turn " << turn;
        Move best_a, best_b;
        double va = 0, vb = 0;
        const long na = walk.GenerateBest(g.board(), rack, &Equity, &leaves, best_a, va);
        const long nb = wmp.GenerateBest(g.board(), rack, &Equity, &leaves, best_b, vb);
        if (na > 0 || nb > 0) {
          ASSERT_GT(na, 0);
          ASSERT_GT(nb, 0);
          EXPECT_EQ(best_a.ToString(), best_b.ToString()) << "seed " << seed << " turn " << turn;
          EXPECT_DOUBLE_EQ(va, vb);
        }
        ++positions;
        moves += static_cast<long>(a.size());
      }
      g.Apply(bot.Choose(g));
    }
  }
  EXPECT_GT(positions, 100);
  EXPECT_GT(moves, 1000);
}

}  // namespace
}  // namespace scrabble
