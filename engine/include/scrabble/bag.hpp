#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "scrabble/rack.hpp"
#include "scrabble/tiles.hpp"

namespace scrabble {

// Tile bag with an explicit seed for reproducible games.
class Bag {
 public:
  explicit Bag(std::uint64_t seed);
  // Bag holding exactly `tiles` (positions reconstructed from a game in progress).
  Bag(const Rack& tiles, std::uint64_t seed);

  int Size() const { return static_cast<int>(tiles_.size()); }
  bool Empty() const { return tiles_.empty(); }

  // Draws until the rack is full (or the bag is empty).
  void Refill(Rack& rack);
  // Returns tiles to the bag (exchanges).
  void Return(Tile t);

 private:
  Tile Draw();

  std::vector<Tile> tiles_;
  std::mt19937_64 rng_;
};

}  // namespace scrabble
