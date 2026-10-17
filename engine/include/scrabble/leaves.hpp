#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "scrabble/rack.hpp"

namespace scrabble {

// Leave values in the KLV format of wolges and Macondo: a KWG whose words are
// leaves (tiles in ascending order, 0 = blank), its arc count first, then the
// number of values and one value per leave in word order (f32 points in KLV2,
// i16 in 1/256 points in the older KLV).
class Leaves {
 public:
  static Leaves FromBytes(std::string_view bytes);
  static Leaves FromFile(const std::string& path);

  // Value in points of keeping `leave`; 0 for the empty leave or one the table lacks.
  double Value(const Rack& leave) const;
  std::size_t Size() const { return keys_.size(); }

 private:
  // Up to 12 tiles, 5 bits each (tile + 1), first tile in the highest bits, so
  // keys sort like the leaves themselves.
  static std::uint64_t Key(const Rack& leave);

  std::vector<std::uint64_t> keys_;  // ascending
  std::vector<float> values_;
};

}  // namespace scrabble
