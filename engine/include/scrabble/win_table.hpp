#pragma once

#include <string>
#include <vector>

namespace scrabble {

// Chance that the player to move wins, by their spread and the tiles they cannot
// see (bag plus opponent's rack), fitted on self-play by scripts/fit_win_table.py.
class WinTable {
 public:
  static constexpr int kMaxSpread = 300;
  static constexpr int kSpreadStep = 5;
  static constexpr int kMaxUnseen = 93;

  // Lines "unseen,spread,win" with every spread step for every unseen count; a
  // first line that is not a number is a header. Throws if a cell is missing.
  static WinTable FromFile(const std::string& path);
  // The same, from the file's text.
  static WinTable FromText(const std::string& text);

  // Interpolated between spread steps; spreads and tile counts out of range are clamped.
  double Win(double spread, int unseen) const;

 private:
  static int SpreadCells() { return 2 * kMaxSpread / kSpreadStep + 1; }
  std::vector<double> cells_;  // by unseen, then by spread
};

}  // namespace scrabble
