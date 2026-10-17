#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace scrabble {

// Encoded tile: 0 = blank ('?'), 1..26 = 'A'..'Z'.
using Tile = std::uint8_t;

inline constexpr Tile kBlank = 0;
inline constexpr int kNumTileTypes = 27;
inline constexpr int kRackSize = 7;

// English Scrabble distribution and values (NWL / CSW).
struct TileInfo {
  std::uint8_t count;
  std::uint8_t score;
};

const std::array<TileInfo, kNumTileTypes>& EnglishTiles();

int TotalTiles();

std::optional<Tile> TileFromChar(char c);
char TileToChar(Tile t);

}  // namespace scrabble
