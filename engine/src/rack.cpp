#include "scrabble/rack.hpp"

namespace scrabble {

std::optional<Rack> Rack::FromString(std::string_view s) {
  Rack r;
  for (char c : s) {
    auto t = TileFromChar(c);
    if (!t) return std::nullopt;
    r.Add(*t);
  }
  return r;
}

void Rack::Add(Tile t) {
  ++counts_[t];
  ++size_;
}

bool Rack::Remove(Tile t) {
  if (counts_[t] == 0) return false;
  --counts_[t];
  --size_;
  return true;
}

std::string Rack::ToString() const {
  std::string out;
  out.reserve(size_);
  for (int t = 0; t < kNumTileTypes; ++t) out.append(counts_[t], TileToChar(static_cast<Tile>(t)));
  return out;
}

}  // namespace scrabble
