#include "scrabble/move.hpp"

#include <algorithm>

namespace scrabble {

int Move::TilesPlayed() const {
  if (kind != Kind::kPlace) return 0;
  return static_cast<int>(
      std::count_if(tiles.begin(), tiles.end(), [](char c) { return c != '.'; }));
}

std::string Move::ToString() const {
  switch (kind) {
    case Kind::kPass:
      return "pass (0)";
    case Kind::kExchange:
      return "exchange " + tiles + " (0)";
    case Kind::kPlace:
      break;
  }
  // GCG-style coordinates: row number first for horizontal plays, column letter first for vertical.
  std::string r = std::to_string(row + 1);
  std::string c(1, static_cast<char>('A' + col));
  std::string coord = dir == Direction::kHorizontal ? r + c : c + r;
  return coord + " " + tiles + " (" + std::to_string(score) + ")";
}

}  // namespace scrabble
