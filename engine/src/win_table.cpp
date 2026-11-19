#include "scrabble/win_table.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace scrabble {

WinTable WinTable::FromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::stringstream text;
  text << in.rdbuf();
  return FromText(text.str());
}

WinTable WinTable::FromText(const std::string& text) {
  WinTable table;
  const std::size_t size = static_cast<std::size_t>(kMaxUnseen + 1) * SpreadCells();
  table.cells_.assign(size, std::numeric_limits<double>::quiet_NaN());
  std::istringstream in(text);
  std::string line;
  std::size_t filled = 0;
  while (std::getline(in, line)) {
    int unseen = 0, spread = 0;
    double win = 0;
    char comma1 = 0, comma2 = 0;
    std::istringstream row(line);
    if (!(row >> unseen >> comma1 >> spread >> comma2 >> win)) continue;  // header
    if (unseen < 0 || unseen > kMaxUnseen || spread < -kMaxSpread || spread > kMaxSpread ||
        spread % kSpreadStep != 0)
      throw std::runtime_error("win table cell out of range: " + line);
    double& cell = table.cells_[static_cast<std::size_t>(unseen) * SpreadCells() +
                                (spread + kMaxSpread) / kSpreadStep];
    if (cell != cell) ++filled;
    cell = win;
  }
  if (filled != size) throw std::runtime_error("win table has missing cells");
  return table;
}

double WinTable::Win(double spread, int unseen) const {
  const int u = std::clamp(unseen, 0, kMaxUnseen);
  const double position =
      (std::clamp(spread, -double(kMaxSpread), double(kMaxSpread)) + kMaxSpread) / kSpreadStep;
  const int low = std::min(static_cast<int>(position), SpreadCells() - 2);
  const double* row = cells_.data() + static_cast<std::size_t>(u) * SpreadCells();
  return row[low] + (position - low) * (row[low + 1] - row[low]);
}

}  // namespace scrabble
