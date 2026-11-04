#pragma once

// Seeds of a self-play run, shared by the apps and the determinism tests: the
// deal of each game and, with per-game seeds, the random streams of the bot in
// each seat, both derived from the run's seed alone.

#include <cstdint>

namespace scrabble {

// SplitMix64: decorrelates per-game seeds so different run seeds share no games.
inline std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

// The bag of deal `deal` (paired runs play each deal twice).
inline std::uint64_t DealSeed(std::uint64_t seed, int deal) {
  return MixSeed(MixSeed(seed) + static_cast<std::uint64_t>(deal));
}

// The streams of the bot sitting in `seat` in deal `deal`: both games of a pair
// give each seat the same streams, whichever bot sits there.
inline std::uint64_t StreamSeed(std::uint64_t seed, int deal, int seat) {
  return MixSeed(MixSeed(seed ^ 0x9e1f5eedULL) + 2 * static_cast<std::uint64_t>(deal) +
                 static_cast<std::uint64_t>(seat));
}

}  // namespace scrabble
