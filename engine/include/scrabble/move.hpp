#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "scrabble/rack.hpp"
#include "scrabble/tiles.hpp"

namespace scrabble {

enum class Direction : std::uint8_t { kHorizontal, kVertical };

struct Move {
  enum class Kind : std::uint8_t { kPlace, kExchange, kPass };

  Kind kind = Kind::kPass;
  // Placement: first square of the main word (including played-through tiles).
  int row = 0;
  int col = 0;
  Direction dir = Direction::kHorizontal;
  // One entry per square of the main word: 'A'-'Z' for a new tile, 'a'-'z' for a
  // new blank, '.' for a tile already on the board. Exchange: the tiles returned.
  std::string tiles;
  int score = 0;

  int TilesPlayed() const;
  // Human-readable form, e.g. "8D QI? (12)" or "H8 c.T (5)" style coordinates.
  std::string ToString() const;

  bool operator==(const Move&) const = default;
};

}  // namespace scrabble
