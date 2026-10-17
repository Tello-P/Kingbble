#include "scrabble/wordmap.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <random>
#include <string>

namespace scrabble {

namespace {

const std::vector<std::string>& SmallWords() {
  static const auto* words = [] {
    auto* w = new std::vector<std::string>;
    std::ifstream in(std::string(KINGBBLE_FIXTURES) + "/enable_small.txt");
    for (std::string s; in >> s;) w->push_back(s);
    return w;
  }();
  return *words;
}

// True if `word` can be spelled with `tiles` ('?' = blank); `exact` requires
// every tile to be used.
bool Spellable(const std::string& word, const std::string& tiles, bool exact) {
  std::array<int, 26> have{};
  int blanks = 0;
  for (char c : tiles) c == '?' ? ++blanks : ++have[c - 'A'];
  for (char c : word) {
    if (have[c - 'A'] > 0) {
      --have[c - 'A'];
    } else if (blanks > 0) {
      --blanks;
    } else {
      return false;
    }
  }
  return !exact || word.size() == tiles.size();
}

std::vector<std::string> BruteForce(const std::string& tiles, bool exact) {
  std::vector<std::string> out;
  for (const auto& w : SmallWords())
    if (Spellable(w, tiles, exact)) out.push_back(w);
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace

TEST(WordMap, GroupsAnagrams) {
  WordMap wm = WordMap::FromWords({"RATES", "STARE", "TEARS", "ASTER", "TASTE", "QI", "x", "A"});
  EXPECT_EQ(wm.NumWords(), 6u);
  EXPECT_EQ(wm.NumKeys(), 3u);
  EXPECT_EQ(wm.Anagrams("SETAR"), (std::vector<std::string>{"ASTER", "RATES", "STARE", "TEARS"}));
  EXPECT_EQ(wm.Anagrams("iq"), (std::vector<std::string>{"QI"}));
  EXPECT_TRUE(wm.Anagrams("ABC").empty());
  EXPECT_TRUE(wm.Anagrams("A1").empty());
  EXPECT_TRUE(wm.Contains("TASTE"));
  EXPECT_FALSE(wm.Contains("SATET"));
}

TEST(WordMap, BlanksMatchAnyLetter) {
  WordMap wm = WordMap::FromWords({"RATES", "STARE", "TASTE", "QI", "XI"});
  EXPECT_EQ(wm.Anagrams("RATE?"), (std::vector<std::string>{"RATES", "STARE"}));
  EXPECT_EQ(wm.Anagrams("?I"), (std::vector<std::string>{"QI", "XI"}));
  EXPECT_EQ(wm.Anagrams("??"), (std::vector<std::string>{"QI", "XI"}));
  EXPECT_EQ(wm.Subanagrams("QIX"), (std::vector<std::string>{"QI", "XI"}));
}

TEST(WordMap, MatchesBruteForceOnRandomRacks) {
  WordMap wm = WordMap::FromWords(SmallWords());
  EXPECT_EQ(wm.NumWords(), SmallWords().size());
  const std::string bag =
      "AAAAAAAAABBCCDDDDEEEEEEEEEEEEFFGGGHHIIIIIIIIIJKLLLLMMNNNNNNOOOOOOOOPPQRRRRRR"
      "SSSSTTTTTTUUUUVVWWXYYZ??";
  std::mt19937 rng(7);
  for (int i = 0; i < 200; ++i) {
    std::string pool = bag;
    std::shuffle(pool.begin(), pool.end(), rng);
    std::string rack = pool.substr(0, i % 2 ? 7 : 5);
    EXPECT_EQ(wm.Anagrams(rack), BruteForce(rack, true)) << rack;
    EXPECT_EQ(wm.Subanagrams(rack), BruteForce(rack, false)) << rack;
  }
}

}  // namespace scrabble
