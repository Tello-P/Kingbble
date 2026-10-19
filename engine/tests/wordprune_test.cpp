#include "scrabble/wordprune.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "scrabble/endgame.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/greedy_bot.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/preendgame.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

class WordPruneTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");

  // Position of a greedy self-play game once the bag holds at most `bag` tiles.
  Game Position(std::uint64_t seed, int bag) {
    GreedyBot bot(gaddag_);
    Game g(seed);
    while (!g.over() && g.bag_size() > bag) g.Apply(bot.Choose(g));
    return g;
  }

  static std::vector<std::string> Sorted(const std::vector<Move>& moves) {
    std::vector<std::string> out;
    for (const Move& m : moves) out.push_back(m.ToString() + " " + std::to_string(m.score));
    std::sort(out.begin(), out.end());
    return out;
  }
};

Board BoardWith(int row, int col, const std::string& word) {
  Board b;
  for (std::size_t i = 0; i < word.size(); ++i)
    b.Place(row, col + static_cast<int>(i), *TileFromChar(word[i]), false);
  return b;
}

bool Has(const std::vector<std::string>& words, const std::string& w) {
  return std::binary_search(words.begin(), words.end(), w);
}

}  // namespace

TEST(PlayableWords, NeedsThePoolForTheEmptySquares) {
  Gaddag g = Gaddag::FromWords({"AT", "CAT", "CATS", "SCAT", "TA", "TACT", "ZA"});
  Board board = BoardWith(7, 6, "CAT");
  auto words = PlayableWords(g, board, *Rack::FromString("S"));
  // CATS and SCAT extend CAT; AT and TA cannot cross CAT with only an S.
  EXPECT_EQ(words, (std::vector<std::string>{"CATS", "SCAT"}));
  words = PlayableWords(g, board, *Rack::FromString("ATT"));
  EXPECT_TRUE(Has(words, "AT"));
  EXPECT_TRUE(Has(words, "TA"));
  EXPECT_TRUE(Has(words, "TACT"));   // down through the C or T of CAT
  EXPECT_TRUE(Has(words, "CAT"));    // down from the C
  EXPECT_FALSE(Has(words, "CATS"));  // no S
  EXPECT_FALSE(Has(words, "ZA"));
  // A blank stands for the Z.
  EXPECT_TRUE(Has(PlayableWords(g, board, *Rack::FromString("?A")), "ZA"));
}

TEST(PlayableWords, RespectsTheTilesAtEitherEnd) {
  Gaddag g = Gaddag::FromWords({"AB", "ABC"});
  Board board;
  board.Place(0, 2, *TileFromChar('C'), false);
  // AB along row 0 would touch the C and read ABC.
  auto words = PlayableWords(g, board, *Rack::FromString("AB"));
  EXPECT_TRUE(Has(words, "ABC"));
  EXPECT_TRUE(Has(words, "AB"));  // still fits elsewhere
  board.Place(0, 0, *TileFromChar('X'), false);
  for (int c = 4; c < kBoardDim; ++c) board.Place(0, c, *TileFromChar('X'), false);
  for (int r = 1; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c) board.Place(r, c, *TileFromChar('X'), false);
  // Row 0 is X _ C X X ...: neither word fits any more.
  EXPECT_EQ(PlayableWords(g, board, *Rack::FromString("AB")), std::vector<std::string>{});
}

TEST_F(WordPruneTest, GeneratesTheSameMovesFromEveryLaterPosition) {
  for (std::uint64_t seed = 1; seed <= 12; ++seed) {
    Game g = Position(seed, 6);
    if (g.over()) continue;
    const Gaddag pruned = PruneLexicon(gaddag_, g.board(), UnseenTiles(g.board(), Rack{}));
    EXPECT_LT(pruned.NumWords(), gaddag_.NumWords());
    MoveGenerator full(gaddag_), small(pruned);
    GreedyBot bot(gaddag_);
    while (!g.over()) {
      for (int p = 0; p < 2; ++p)
        ASSERT_EQ(Sorted(full.Generate(g.board(), g.rack(p))),
                  Sorted(small.Generate(g.board(), g.rack(p))))
            << "seed " << seed;
      g.Apply(bot.Choose(g));
    }
  }
}

TEST_F(WordPruneTest, EndgameSolverGivesTheSameValues) {
  EndgameSolver pruned(gaddag_), full(gaddag_);
  full.set_pruning(false);
  GreedyBot bot(gaddag_);
  int solved = 0;
  for (std::uint64_t seed = 1; solved < 10 && seed < 100; ++seed) {
    Game g = Position(seed, 0);
    while (!g.over() && g.rack(0).Size() + g.rack(1).Size() > 9) g.Apply(bot.Choose(g));
    if (g.over()) continue;
    EndgameResult a = pruned.Solve(g), b = full.Solve(g);
    ASSERT_TRUE(a.exact && b.exact);
    EXPECT_EQ(a.value, b.value) << "seed " << seed;
    EXPECT_EQ(a.move, b.move) << "seed " << seed;
    ++solved;
  }
  EXPECT_EQ(solved, 10);
}

TEST_F(WordPruneTest, PreEndgameSolverGivesTheSameResults) {
  Leaves leaves = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  PreEndgameSolver pruned(gaddag_, leaves), full(gaddag_, leaves);
  full.set_pruning(false);
  PreEndgameParams params;
  params.candidates = 6;
  params.endgame_plies = 4;
  for (std::uint64_t seed = 1; seed <= 3; ++seed) {
    Game g = Position(seed, 1);
    if (g.over() || g.bag_size() != 1) continue;
    PreEndgameResult a = pruned.Solve(g, params), b = full.Solve(g, params);
    ASSERT_EQ(a.plays.size(), b.plays.size());
    EXPECT_EQ(a.endgames, b.endgames);
    for (std::size_t i = 0; i < a.plays.size(); ++i) {
      EXPECT_EQ(a.plays[i].move, b.plays[i].move) << "seed " << seed;
      EXPECT_EQ(a.plays[i].wins, b.plays[i].wins);
      EXPECT_EQ(a.plays[i].spread, b.plays[i].spread);
    }
  }
}

}  // namespace scrabble
