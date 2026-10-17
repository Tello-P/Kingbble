#include "scrabble/bag.hpp"

#include <gtest/gtest.h>

namespace scrabble {

TEST(Bag, StartsFull) { EXPECT_EQ(Bag(1).Size(), 100); }

TEST(Bag, RefillDrawsSevenTiles) {
  Bag bag(42);
  Rack rack;
  bag.Refill(rack);
  EXPECT_EQ(rack.Size(), kRackSize);
  EXPECT_EQ(bag.Size(), 93);
}

TEST(Bag, SameSeedSameDraws) {
  Bag a(7), b(7);
  Rack ra, rb;
  a.Refill(ra);
  b.Refill(rb);
  EXPECT_EQ(ra, rb);
}

TEST(Bag, DrainingPreservesDistribution) {
  Bag bag(3);
  std::array<int, kNumTileTypes> seen{};
  while (!bag.Empty()) {
    Rack rack;
    bag.Refill(rack);
    for (int t = 0; t < kNumTileTypes; ++t) seen[t] += rack.Count(Tile(t));
  }
  for (int t = 0; t < kNumTileTypes; ++t) EXPECT_EQ(seen[t], EnglishTiles()[t].count);
}

}  // namespace scrabble
