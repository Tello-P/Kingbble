#include "scrabble/rack.hpp"

#include <gtest/gtest.h>

namespace scrabble {

TEST(Rack, CanonicalString) {
  auto r = Rack::FromString("TSENIA?");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->ToString(), "?AEINST");
  EXPECT_EQ(r->Size(), 7);
}

TEST(Rack, RemoveMissingTileFails) {
  auto r = *Rack::FromString("AB");
  EXPECT_FALSE(r.Remove(*TileFromChar('C')));
  EXPECT_TRUE(r.Remove(*TileFromChar('A')));
  EXPECT_EQ(r.ToString(), "B");
}

TEST(Rack, RejectsInvalidChars) { EXPECT_FALSE(Rack::FromString("AB1").has_value()); }

}  // namespace scrabble
