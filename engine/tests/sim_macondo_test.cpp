#include <gtest/gtest.h>

#include <iomanip>
#include <sstream>
#include <string>

#include "scrabble/equity_bot.hpp"
#include "scrabble/game.hpp"
#include "scrabble/sim_bot.hpp"

namespace scrabble {
namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

// A table in Macondo's layout whose cell is a function of row and column, so
// each lookup can be checked against the cell Macondo's addWinPctStat reads.
std::string Table() {
  std::ostringstream out;
  out << std::setprecision(9);
  for (int u = 0; u < 94; ++u) out << "," << u;
  out << "\n";
  for (int row = 0; row <= 600; ++row) {
    out << 300 - row;
    for (int u = 0; u < 94; ++u) out << "," << (row * 94 + u) / (601.0 * 94.0);
    out << "\n";
  }
  return out.str();
}

double Cell(int row, int u) { return static_cast<float>((row * 94 + u) / (601.0 * 94.0)); }

TEST(MacondoWinPct, ReadsMacondosLayout) {
  auto t = MacondoWinPctFromText(Table());
  ASSERT_EQ(t->size(), 601u * 94u);
  EXPECT_NEAR((*t)[0], Cell(0, 0), 1e-6);
  EXPECT_NEAR((*t)[600 * 94 + 93], Cell(600, 93), 1e-6);
  EXPECT_THROW(MacondoWinPctFromText(",0\n300,1\n"), std::runtime_error);
}

TEST(MacondoWinPct, LooksUpAsAddWinPctStat) {
  auto t = MacondoWinPctFromText(Table());
  // Odd plies: we are to move; row 300 - (spread + round(leftover)).
  EXPECT_NEAR(MacondoPlayoutWin(*t, 20, 4.6, false, 30, false), Cell(300 - 25, 30), 1e-6);
  // Even plies: the opponent is to move; their spread, then one minus.
  EXPECT_NEAR(MacondoPlayoutWin(*t, 20, 4.4, false, 30, true), 1 - Cell(300 + 24, 30), 1e-6);
  // Clamped spread and unseen count.
  EXPECT_NEAR(MacondoPlayoutWin(*t, 900, 0, false, 120, false), Cell(0, 93), 1e-6);
  EXPECT_NEAR(MacondoPlayoutWin(*t, -900, 0, false, 5, false), Cell(600, 5), 1e-6);
  // A finished game, or no tiles unseen, counts the result.
  EXPECT_EQ(MacondoPlayoutWin(*t, 3, -50, true, 10, true), 1.0);
  EXPECT_EQ(MacondoPlayoutWin(*t, 0, 9, false, 0, false), 0.5);
  EXPECT_EQ(MacondoPlayoutWin(*t, -1, 0, true, 0, false), 0.0);
}

TEST(MacondoStyle, PlaysALegalMoveTheSameWayEveryTime) {
  Gaddag gaddag = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  Game g(3);
  EquityBot equity(gaddag, leaves);
  for (int i = 0; i < 4 && !g.over(); ++i) g.Apply(equity.Choose(g));
  SimParams p;
  p.macondo_style = true;
  p.win_pct = MacondoWinPctFromText(Table());
  p.candidates = 8;
  p.threads = 2;
  p.seed = 5;
  p.time_limit_s = 0.3;
  p.end_unseen = 14;
  p.late_bag = 7;
  SimBot a(gaddag, leaves, p);
  const Move m = a.Choose(g);
  Game after = g;
  EXPECT_NO_THROW(after.Apply(m));
  ASSERT_FALSE(a.last_results().empty());
  EXPECT_EQ(a.last_results().front().move.ToString(), m.ToString());
  EXPECT_FALSE(a.last_results().front().pruned);
  EXPECT_GT(a.last_results().front().iterations, 0);
}

}  // namespace
}  // namespace scrabble
