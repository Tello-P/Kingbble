#include "scrabble/move.hpp"

#include <algorithm>

namespace scrabble {

int Move::TilesPlayed() const {
  if (kind != Kind::kPlace) return 0;
  return static_cast<int>(
      std::count_if(tiles.begin(), tiles.end(), [](char c) { return c != '.'; }));
}

bool TiebreaksBetter(const Move& a, const Move& b) {
  if (a.score != b.score) return a.score > b.score;
  if (a.kind != b.kind) return a.kind < b.kind;
  if (a.row != b.row) return a.row < b.row;
  if (a.col != b.col) return a.col < b.col;
  if (a.dir != b.dir) return a.dir == Direction::kHorizontal;
  // Exchanges count the tiles returned, as Macondo does.
  auto tiles = [](const Move& m) {
    return m.kind == Move::Kind::kExchange ? static_cast<int>(m.tiles.size()) : m.TilesPlayed();
  };
  if (tiles(a) != tiles(b)) return tiles(a) < tiles(b);
  if (a.tiles.size() != b.tiles.size()) return a.tiles.size() < b.tiles.size();
  return a.tiles < b.tiles;
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
