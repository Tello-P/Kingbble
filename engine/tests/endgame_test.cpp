#include "scrabble/endgame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "scrabble/endgame_bot.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/greedy_bot.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

// Plain minimax over the same rules (no pruning, no table): the reference value.
int BruteForce(MoveGenerator& gen, Board& board, std::array<Rack, 2>& racks, int me,
               int scoreless) {
  std::vector<Move> moves = gen.Generate(board, racks[me]);
  moves.emplace_back();
  int best = -1000000;
  for (const Move& m : moves) {
    int gained = 0;
    std::vector<std::pair<int, int>> placed;
    if (m.kind == Move::Kind::kPlace) {
      gained = ScoreMove(board, m);
      const int dr = m.dir == Direction::kVertical ? 1 : 0;
      for (std::size_t k = 0; k < m.tiles.size(); ++k) {
        char ch = m.tiles[k];
        if (ch == '.') continue;
        bool blank = std::islower(static_cast<unsigned char>(ch));
        int r = m.row + dr * static_cast<int>(k), c = m.col + (1 - dr) * static_cast<int>(k);
        racks[me].Remove(blank ? kBlank : *TileFromChar(ch));
        board.Place(r, c, *TileFromChar(ch), blank);
        placed.emplace_back(r, c);
      }
    }
    const int sl = gained > 0 ? 0 : scoreless + 1;
    int value;
    if (racks[me].Empty()) {
      value = gained + 2 * RackValue(racks[1 - me]);
    } else if (sl >= 2) {
      value = gained - RackValue(racks[me]) + RackValue(racks[1 - me]);
    } else {
      value = gained - BruteForce(gen, board, racks, 1 - me, sl);
    }
    for (auto [r, c] : placed) {
      std::uint8_t cell = board.At(r, c);
      racks[me].Add((cell & Board::kBlankBit) ? kBlank : Tile(cell & 0x7F));
      board.Clear(r, c);
    }
    best = std::max(best, value);
  }
  return best;
}

// Empty-bag positions from greedy self-play with at most `max_tiles` tiles on both racks.
std::vector<Game> SmallEndgames(const Gaddag& gaddag, int count, int max_tiles) {
  std::vector<Game> out;
  GreedyBot bot(gaddag);
  for (std::uint64_t seed = 1; static_cast<int>(out.size()) < count && seed < 500; ++seed) {
    Game g(seed);
    while (!g.over() && (g.bag_size() > 0 || g.rack(0).Size() + g.rack(1).Size() > max_tiles))
      g.Apply(bot.Choose(g));
    if (!g.over()) out.push_back(g);
  }
  return out;
}

class EndgameTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");
};

}  // namespace

TEST_F(EndgameTest, MatchesBruteForceMinimax) {
  std::vector<Game> positions = SmallEndgames(gaddag_, 10, 5);
  ASSERT_GE(positions.size(), 8u);
  EndgameSolver solver(gaddag_);
  MoveGenerator gen(gaddag_);
  for (const Game& g : positions) {
    EndgameResult r = solver.Solve(g);
    EXPECT_TRUE(r.exact);
    Board board = g.board();
    std::array<Rack, 2> racks{g.rack(0), g.rack(1)};
    EXPECT_EQ(r.value, BruteForce(gen, board, racks, g.to_move(), 0))
        << g.rack(0).ToString() << " / " << g.rack(1).ToString();
  }
}

TEST_F(EndgameTest, BestMoveAchievesTheValue) {
  EndgameSolver solver(gaddag_);
  for (const Game& g : SmallEndgames(gaddag_, 8, 8)) {
    EndgameResult r = solver.Solve(g);
    ASSERT_TRUE(r.exact);
    ASSERT_FALSE(r.pv.empty());
    EXPECT_EQ(r.pv.front(), r.move);
    Game next = g;
    const int me = g.to_move();
    const int before = next.score(me) - next.score(1 - me);
    next.Apply(r.move);  // throws if illegal
    const int gained = next.score(me) - next.score(1 - me) - before;
    if (next.over()) {
      EXPECT_EQ(r.value, gained);
    } else if (r.move.kind != Move::Kind::kPass) {
      EXPECT_EQ(r.value, gained - solver.Solve(next).value);
    }
  }
}

TEST_F(EndgameTest, RespectsTheTimeLimit) {
  std::vector<Game> positions = SmallEndgames(gaddag_, 1, 14);
  ASSERT_FALSE(positions.empty());
  EndgameSolver solver(gaddag_);
  EndgameResult r = solver.Solve(positions[0], 25, 1e-6);
  EXPECT_GE(r.depth, 1);
  Game g = positions[0];
  g.Apply(r.move);
}

TEST_F(EndgameTest, NeedsAnEmptyBag) {
  EndgameSolver solver(gaddag_);
  EXPECT_THROW(solver.Solve(Game(1)), std::logic_error);
}

TEST_F(EndgameTest, EndgameBotPlaysFullGames) {
  Leaves leaves = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  EndgameBot bot(std::make_unique<EquityBot>(gaddag_, leaves), gaddag_, 0.5);
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
