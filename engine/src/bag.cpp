#include "scrabble/bag.hpp"

namespace scrabble {

Bag::Bag(std::uint64_t seed) : rng_(seed) {
  const auto& info = EnglishTiles();
  for (int t = 0; t < kNumTileTypes; ++t) tiles_.insert(tiles_.end(), info[t].count, Tile(t));
}

Tile Bag::Draw() {
  std::uniform_int_distribution<std::size_t> dist(0, tiles_.size() - 1);
  std::size_t i = dist(rng_);
  Tile t = tiles_[i];
  tiles_[i] = tiles_.back();
  tiles_.pop_back();
  return t;
}

void Bag::Refill(Rack& rack) {
  while (rack.Size() < kRackSize && !tiles_.empty()) rack.Add(Draw());
}

void Bag::Return(Tile t) { tiles_.push_back(t); }

}  // namespace scrabble
