#include "scrabble/tiles.hpp"

namespace scrabble {

const std::array<TileInfo, kNumTileTypes>& EnglishTiles() {
  static constexpr std::array<TileInfo, kNumTileTypes> kTiles = {{
      {2, 0},                                                    // ?
      {9, 1}, {2, 3}, {2, 3},  {4, 2}, {12, 1}, {2, 4}, {3, 2},  // A-G
      {2, 4}, {9, 1}, {1, 8},  {1, 5}, {4, 1},  {2, 3}, {6, 1},  // H-N
      {8, 1}, {2, 3}, {1, 10}, {6, 1}, {4, 1},  {6, 1}, {4, 1},  // O-U
      {2, 4}, {2, 4}, {1, 8},  {2, 4}, {1, 10},                  // V-Z
  }};
  return kTiles;
}

int TotalTiles() {
  int total = 0;
  for (const auto& t : EnglishTiles()) total += t.count;
  return total;
}

std::optional<Tile> TileFromChar(char c) {
  if (c == '?') return kBlank;
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  if (c >= 'A' && c <= 'Z') return static_cast<Tile>(c - 'A' + 1);
  return std::nullopt;
}

char TileToChar(Tile t) { return t == kBlank ? '?' : static_cast<char>('A' + t - 1); }

}  // namespace scrabble
