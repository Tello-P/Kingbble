#include "scrabble/win_table.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>
#include <string>

#include "scrabble/equity.hpp"
#include "scrabble/game.hpp"
#include "scrabble/sim_bot.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

// Win grows linearly from 0 at -300 to 1 at +300, the same for every tile count.
std::string LinearTable() {
  std::ostringstream out;
  out << "unseen,spread,win\n";
  for (int u = 0; u <= WinTable::kMaxUnseen; ++u)
    for (int s = -WinTable::kMaxSpread; s <= WinTable::kMaxSpread; s += WinTable::kSpreadStep)
      out << u << "," << s << "," << (s + 300) / 600.0 << "\n";
  return out.str();
}

TEST(WinTable, InterpolatesBetweenSteps) {
  const WinTable table = WinTable::FromText(LinearTable());
  EXPECT_NEAR(table.Win(0, 20), 0.5, 1e-5);
  EXPECT_NEAR(table.Win(2.5, 20), 0.5 + 2.5 / 600, 1e-5);
  EXPECT_NEAR(table.Win(-299, 20), 1.0 / 600, 1e-5);
}

TEST(WinTable, ClampsSpreadAndTiles) {
  const WinTable table = WinTable::FromText(LinearTable());
  EXPECT_NEAR(table.Win(1000, 20), 1, 1e-9);
  EXPECT_NEAR(table.Win(-1000, 20), 0, 1e-9);
  EXPECT_NEAR(table.Win(0, 200), 0.5, 1e-9);
  EXPECT_NEAR(table.Win(0, -3), 0.5, 1e-9);
}

TEST(WinTable, MissingCellsAreRefused) {
  EXPECT_THROW(WinTable::FromText("unseen,spread,win\n0,0,0.5\n"), std::runtime_error);
}

class SimWithTableTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves_ = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  WinTable table_ = WinTable::FromText(LinearTable());

  SimParams Params() const {
    SimParams p;
    p.candidates = 5;
    p.max_iterations = 24;
    p.min_iterations = 8;
    p.batch = 8;
    p.seed = 7;
    return p;
  }

  Game MidGame() {
    Game g(3);
    EquityBot bot(gaddag_, leaves_);
    for (int i = 0; i < 4 && !g.over(); ++i) g.Apply(bot.Choose(g));
    return g;
  }
};

TEST_F(SimWithTableTest, MeansAreChancesAndSpreadsAreKept) {
  SimParams p = Params();
  p.win_table = &table_;
  SimBot bot(gaddag_, leaves_, p);
  bot.Choose(MidGame());
  ASSERT_FALSE(bot.last_results().empty());
  for (const SimResult& r : bot.last_results()) {
    EXPECT_GE(r.mean, 0);
    EXPECT_LE(r.mean, 1);
  }
}

TEST_F(SimWithTableTest, WithALinearTableItMatchesTheSpreadChoice) {
  // A table linear in the spread ranks moves as the mean spread does.
  const Game game = MidGame();
  SimParams p = Params();
  p.prune_z = 1e9;
  SimBot plain(gaddag_, leaves_, p);
  p.win_table = &table_;
  SimBot with_table(gaddag_, leaves_, p);
  EXPECT_EQ(plain.Choose(game), with_table.Choose(game));
}

TEST_F(SimWithTableTest, WithoutATableTheSimulationIsUnchanged) {
  const Game game = MidGame();
  SimBot bot(gaddag_, leaves_, Params());
  bot.Choose(game);
  for (const SimResult& r : bot.last_results()) EXPECT_DOUBLE_EQ(r.mean, r.mean_spread);
}

}  // namespace

}  // namespace scrabble
