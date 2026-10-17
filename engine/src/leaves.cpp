#include "scrabble/leaves.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

#include "scrabble/kwg.hpp"

namespace scrabble {

namespace {

constexpr int kKeyBits = 5;
constexpr int kMaxKeyTiles = 12;

std::uint32_t ReadU32(std::string_view b, std::size_t at) {
  const auto* p = reinterpret_cast<const unsigned char*>(b.data() + at);
  return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
         static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}

std::uint64_t PadKey(std::uint64_t key, int n) { return key << (kKeyBits * (kMaxKeyTiles - n)); }

// Appends the key of every leave under `list`, in word order.
void CollectKeys(const Kwg& kwg, std::uint32_t list, std::uint64_t prefix, int depth,
                 std::vector<std::uint64_t>& out) {
  if (depth >= kMaxKeyTiles) throw std::runtime_error("malformed KLV: leave too long");
  for (std::uint32_t i = list;; ++i) {
    if (i >= kwg.NumArcs()) throw std::runtime_error("malformed KLV");
    Kwg::Arc a = kwg.At(i);
    if (Kwg::Tile(a) >= kNumTileTypes) throw std::runtime_error("malformed KLV: bad tile");
    std::uint64_t key = prefix << kKeyBits | (Kwg::Tile(a) + 1u);
    if (Kwg::Accepts(a)) out.push_back(PadKey(key, depth + 1));
    if (Kwg::Children(a) != Kwg::kNoChildren)
      CollectKeys(kwg, Kwg::Children(a), key, depth + 1, out);
    if (Kwg::IsEnd(a)) break;
  }
}

}  // namespace

std::uint64_t Leaves::Key(const Rack& leave) {
  std::uint64_t key = 0;
  for (int t = 0; t < kNumTileTypes; ++t)
    for (int n = 0; n < leave.Count(Tile(t)); ++n) key = key << kKeyBits | (t + 1u);
  return PadKey(key, leave.Size());
}

Leaves Leaves::FromBytes(std::string_view bytes) {
  if (bytes.size() < 4) throw std::runtime_error("malformed KLV");
  std::size_t kwg_size = std::size_t{ReadU32(bytes, 0)} * 4;
  if (bytes.size() < 8 + kwg_size) throw std::runtime_error("malformed KLV");
  Kwg kwg = Kwg::FromBytes(bytes.substr(4, kwg_size));
  std::size_t count = ReadU32(bytes, 4 + kwg_size);
  std::string_view values = bytes.substr(8 + kwg_size);
  bool klv2 = values.size() == count * 4;
  if (!klv2 && values.size() != count * 2) throw std::runtime_error("malformed KLV: value count");

  Leaves l;
  if (kwg.DawgRoot() != Kwg::kNoChildren) CollectKeys(kwg, kwg.DawgRoot(), 0, 0, l.keys_);
  if (l.keys_.size() != count) throw std::runtime_error("malformed KLV: leave count");
  l.values_.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    if (klv2) {
      l.values_[i] = std::bit_cast<float>(ReadU32(values, 4 * i));
    } else {
      const auto* p = reinterpret_cast<const unsigned char*>(values.data() + 2 * i);
      auto raw = static_cast<std::int16_t>(p[0] | p[1] << 8);
      l.values_[i] = static_cast<float>(raw) / 256.0f;
    }
  }
  if (!std::is_sorted(l.keys_.begin(), l.keys_.end())) {
    std::vector<std::size_t> order(count);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return l.keys_[a] < l.keys_[b]; });
    Leaves sorted;
    for (std::size_t i : order) {
      sorted.keys_.push_back(l.keys_[i]);
      sorted.values_.push_back(l.values_[i]);
    }
    return sorted;
  }
  return l;
}

Leaves Leaves::FromFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open KLV: " + path);
  std::stringstream ss;
  ss << in.rdbuf();
  return FromBytes(ss.str());
}

double Leaves::Value(const Rack& leave) const {
  if (leave.Empty() || leave.Size() > kMaxKeyTiles) return 0;
  std::uint64_t key = Key(leave);
  auto it = std::lower_bound(keys_.begin(), keys_.end(), key);
  if (it == keys_.end() || *it != key) return 0;
  return values_[static_cast<std::size_t>(it - keys_.begin())];
}

}  // namespace scrabble
