#include "scrabble/leaves.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace scrabble {

namespace {

// Built from leaves_small.csv with wolges: buildlex klv [--klv 1] leaves_small.csv OUT
const std::string kFixtures = KINGBBLE_FIXTURES;

double V(const Leaves& l, const char* leave) { return l.Value(*Rack::FromString(leave)); }

}  // namespace

TEST(Leaves, LoadsKlv2) {
  Leaves l = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  EXPECT_EQ(l.Size(), 10u);
  EXPECT_DOUBLE_EQ(V(l, "?"), 25.5);
  EXPECT_DOUBLE_EQ(V(l, "A"), 1.25);
  EXPECT_DOUBLE_EQ(V(l, "BA"), -0.5);
  EXPECT_DOUBLE_EQ(V(l, "CBA"), -3);
  EXPECT_DOUBLE_EQ(V(l, "Q"), -7.75);
  EXPECT_DOUBLE_EQ(V(l, "UQ"), -2);
  EXPECT_DOUBLE_EQ(V(l, "S?"), 30);
  EXPECT_DOUBLE_EQ(V(l, "STIRE"), 12.5);
  EXPECT_DOUBLE_EQ(V(l, "EEEEEE"), -30);
}

TEST(Leaves, MissingLeavesAreWorthZero) {
  Leaves l = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  EXPECT_EQ(V(l, ""), 0);
  EXPECT_EQ(V(l, "B"), 0);
  EXPECT_EQ(V(l, "AA"), 0);
  EXPECT_EQ(V(l, "ABCD"), 0);
  EXPECT_EQ(V(l, "EEEEEEE"), 0);
}

TEST(Leaves, LoadsKlvInSixteenthsOfAPoint) {
  Leaves l = Leaves::FromFile(kFixtures + "/leaves_small.klv");
  EXPECT_EQ(l.Size(), 10u);
  EXPECT_DOUBLE_EQ(V(l, "?"), 25.5);
  EXPECT_DOUBLE_EQ(V(l, "Q"), -7.75);
  EXPECT_DOUBLE_EQ(V(l, "EIRST"), 12.5);
}

TEST(Leaves, RejectsMalformedInput) {
  EXPECT_THROW(Leaves::FromBytes(""), std::runtime_error);
  EXPECT_THROW(Leaves::FromBytes(std::string("\x05\x00\x00\x00", 4)), std::runtime_error);
}

// Published NWL23 leaves (wolges/Macondo), kept outside git: lexica/NWL23.klv2.
TEST(Leaves, PublishedTableIsPlausible) {
  std::string path = kFixtures + "/../NWL23.klv2";
  if (!std::filesystem::exists(path)) GTEST_SKIP() << "no " << path;
  Leaves l = Leaves::FromFile(path);
  EXPECT_GT(l.Size(), 900000u);
  EXPECT_GT(V(l, "?"), 20);
  EXPECT_GT(V(l, "S"), V(l, "V"));
  EXPECT_LT(V(l, "Q"), 0);
  EXPECT_GT(V(l, "AEINST"), V(l, "AEIOUU"));
}

}  // namespace scrabble
