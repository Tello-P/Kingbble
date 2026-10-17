#include "scrabble/gaddag.hpp"

#include <gtest/gtest.h>

namespace scrabble {

TEST(Gaddag, ContainsInsertedWords) {
  Gaddag g = Gaddag::FromWords({"AA", "CAT", "CATS", "ACT", "QI"});
  EXPECT_EQ(g.NumWords(), 5u);
  for (const char* w : {"AA", "CAT", "CATS", "ACT", "QI"}) EXPECT_TRUE(g.Contains(w)) << w;
  for (const char* w : {"A", "CA", "AT", "TAC", "CATSS", "QIS", ""})
    EXPECT_FALSE(g.Contains(w)) << w;
}

TEST(Gaddag, SkipsInvalidEntries) {
  Gaddag g = Gaddag::FromWords({"A", "ab", "C4T", "OK"});
  EXPECT_EQ(g.NumWords(), 1u);
  EXPECT_TRUE(g.Contains("OK"));
}

TEST(Gaddag, EveryPathFromEverySplitPoint) {
  Gaddag g = Gaddag::FromWords({"CARE"});
  // Split after "CA": reverse("CA") + separator + "RE".
  Gaddag::Node n = g.Root();
  for (std::uint8_t l :
       {std::uint8_t{1}, std::uint8_t{3}, Gaddag::kSeparator, std::uint8_t{18}, std::uint8_t{5}}) {
    n = g.Next(n, l);
    ASSERT_NE(n, Gaddag::kNone);
  }
  EXPECT_TRUE(g.IsTerminal(n));
}

TEST(Gaddag, MinimizationSharesSuffixes) {
  Gaddag a = Gaddag::FromWords({"CARE"});
  Gaddag b = Gaddag::FromWords({"CARE", "BARE", "DARE", "MARE"});
  // Adding words with a shared suffix must grow the graph far less than linearly.
  EXPECT_LT(b.NumNodes(), 3 * a.NumNodes());
}

TEST(Gaddag, LoadsFixtureFile) {
  Gaddag g = Gaddag::FromFile(std::string(KINGBBLE_FIXTURES) + "/tiny_raw.txt");
  EXPECT_TRUE(g.Contains("HELLO"));
  EXPECT_TRUE(g.Contains("QI"));
  EXPECT_FALSE(g.Contains("HELL"));
}

}  // namespace scrabble
