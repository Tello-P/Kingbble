#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "scrabble/tiles.hpp"

namespace scrabble {

// Rack as a multiset: one counter per tile type.
class Rack {
 public:
  Rack() = default;

  static std::optional<Rack> FromString(std::string_view s);

  void Add(Tile t);
  bool Remove(Tile t);
  int Count(Tile t) const { return counts_[t]; }
  int Size() const { return size_; }
  bool Empty() const { return size_ == 0; }

  // Canonical sorted representation: blanks first ("?AEINST").
  std::string ToString() const;

  bool operator==(const Rack&) const = default;

 private:
  std::array<std::uint8_t, kNumTileTypes> counts_{};
  int size_ = 0;
};

}  // namespace scrabble
