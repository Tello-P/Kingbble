#pragma once

// The per-rack sums kingbble_leavegen writes and kingbble_leavefit reads.

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "scrabble/rack.hpp"

namespace leavegen {

using namespace scrabble;

constexpr int kBuckets = 4;
// Bag ranges: 7-20, 21-40, 41-60, 61 and more.
int Bucket(int bag) { return bag <= 20 ? 0 : bag <= 40 ? 1 : bag <= 60 ? 2 : 3; }

struct Agg {
  std::uint32_t n = 0;
  std::array<std::uint32_t, kBuckets> nb{};
  double a = 0, b = 0, c = 0;
};

struct Totals {
  std::array<std::uint64_t, kBuckets> n{};
  std::array<double, kBuckets> a{}, b{}, c{}, aa{}, bb{}, cc{};  // sums and sums of squares
};

std::uint64_t RackKey(const Rack& rack) {
  std::uint64_t key = 0;
  for (int t = 0; t < kNumTileTypes; ++t)
    for (int k = 0; k < rack.Count(Tile(t)); ++k)
      key = key << 5 | static_cast<std::uint64_t>(t + 1);
  return key;
}

std::uint64_t Mix(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

void Write(const std::string& path, const std::unordered_map<std::uint64_t, Agg>& racks,
           const Totals& totals) {
  std::ofstream out(path, std::ios::binary);
  out.write("KBLG1", 5);
  const std::uint32_t buckets = kBuckets;
  out.write(reinterpret_cast<const char*>(&buckets), 4);
  out.write(reinterpret_cast<const char*>(&totals), sizeof totals);
  const std::uint64_t count = racks.size();
  out.write(reinterpret_cast<const char*>(&count), 8);
  for (const auto& [key, agg] : racks) {
    out.write(reinterpret_cast<const char*>(&key), 8);
    out.write(reinterpret_cast<const char*>(&agg.n), 4);
    out.write(reinterpret_cast<const char*>(agg.nb.data()), 4 * kBuckets);
    out.write(reinterpret_cast<const char*>(&agg.a), 8);
    out.write(reinterpret_cast<const char*>(&agg.b), 8);
    out.write(reinterpret_cast<const char*>(&agg.c), 8);
  }
  if (!out) {
    std::cerr << "cannot write " << path << "\n";
    std::exit(1);
  }
}

void Read(const std::string& path, std::unordered_map<std::uint64_t, Agg>& racks, Totals& totals) {
  std::ifstream in(path, std::ios::binary);
  char magic[5];
  std::uint32_t buckets = 0;
  if (!in.read(magic, 5) || std::memcmp(magic, "KBLG1", 5) != 0 ||
      !in.read(reinterpret_cast<char*>(&buckets), 4) || buckets != kBuckets) {
    std::cerr << "not a leavegen file: " << path << "\n";
    std::exit(1);
  }
  Totals t;
  in.read(reinterpret_cast<char*>(&t), sizeof t);
  for (int i = 0; i < kBuckets; ++i) {
    totals.n[i] += t.n[i];
    totals.a[i] += t.a[i];
    totals.b[i] += t.b[i];
    totals.c[i] += t.c[i];
    totals.aa[i] += t.aa[i];
    totals.bb[i] += t.bb[i];
    totals.cc[i] += t.cc[i];
  }
  std::uint64_t count = 0;
  in.read(reinterpret_cast<char*>(&count), 8);
  for (std::uint64_t r = 0; r < count; ++r) {
    std::uint64_t key = 0;
    Agg g;
    in.read(reinterpret_cast<char*>(&key), 8);
    in.read(reinterpret_cast<char*>(&g.n), 4);
    in.read(reinterpret_cast<char*>(g.nb.data()), 4 * kBuckets);
    in.read(reinterpret_cast<char*>(&g.a), 8);
    in.read(reinterpret_cast<char*>(&g.b), 8);
    in.read(reinterpret_cast<char*>(&g.c), 8);
    Agg& dst = racks[key];
    dst.n += g.n;
    for (int i = 0; i < kBuckets; ++i) dst.nb[i] += g.nb[i];
    dst.a += g.a;
    dst.b += g.b;
    dst.c += g.c;
  }
  if (!in) {
    std::cerr << "truncated leavegen file: " << path << "\n";
    std::exit(1);
  }
}

}  // namespace leavegen
