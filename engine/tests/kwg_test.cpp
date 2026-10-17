#include "scrabble/kwg.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <string>

namespace scrabble {

namespace {

std::vector<std::string> SmallWords() {
  std::ifstream in(std::string(KINGBBLE_FIXTURES) + "/enable_small.txt");
  std::vector<std::string> words;
  for (std::string s; in >> s;) words.push_back(s);
  return words;
}

// Every path accepted by the KWG's GADDAG, with '@' for the separator.
std::vector<std::string> GaddagPaths(const Kwg& k) {
  std::vector<std::string> out;
  std::string path;
  std::function<void(std::uint32_t)> walk = [&](std::uint32_t list) {
    for (std::uint32_t i = list;; ++i) {
      Kwg::Arc a = k.At(i);
      path.push_back(Kwg::Tile(a) == 0 ? '@' : static_cast<char>('A' + Kwg::Tile(a) - 1));
      if (Kwg::Accepts(a)) out.push_back(path);
      if (Kwg::Children(a) != Kwg::kNoChildren) walk(Kwg::Children(a));
      path.pop_back();
      if (Kwg::IsEnd(a)) break;
    }
  };
  walk(k.GaddagRoot());
  return out;
}

std::vector<std::string> ExpectedPaths(const std::vector<std::string>& words) {
  std::vector<std::string> out;
  for (const auto& w : words) {
    for (std::size_t i = 1; i <= w.size(); ++i) {
      std::string p(w.rend() - static_cast<std::ptrdiff_t>(i), w.rend());
      if (i < w.size()) p += "@" + w.substr(i);
      out.push_back(p);
    }
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

}  // namespace

TEST(Kwg, BuildsDawgAndGaddag) {
  Kwg k = Kwg::FromWords({"CARE", "CARES", "CAT", "QI", "x", "A"});
  EXPECT_EQ(k.Words(), (std::vector<std::string>{"CARE", "CARES", "CAT", "QI"}));
  for (const char* w : {"CARE", "CARES", "CAT", "QI"}) EXPECT_TRUE(k.Contains(w)) << w;
  for (const char* w : {"CAR", "CA", "CATS", "Q", "", "care"}) EXPECT_FALSE(k.Contains(w)) << w;
  EXPECT_TRUE(k.GaddagAccepts("ERAC"));
  EXPECT_TRUE(k.GaddagAccepts("AC@RE"));
  EXPECT_TRUE(k.GaddagAccepts("C@ARES"));
  EXPECT_FALSE(k.GaddagAccepts("AC@R"));
  EXPECT_FALSE(k.GaddagAccepts("CARE"));
  EXPECT_EQ(Kwg::Tile(k.At(0)), 0);
  EXPECT_TRUE(Kwg::IsEnd(k.At(0)));
  EXPECT_TRUE(Kwg::IsEnd(k.At(1)));
}

TEST(Kwg, SiblingListsAreSortedAndTerminated) {
  Kwg k = Kwg::FromWords(SmallWords());
  for (std::uint32_t i = 2; i < k.NumArcs(); ++i) {
    if (!Kwg::IsEnd(k.At(i))) EXPECT_LT(Kwg::Tile(k.At(i)), Kwg::Tile(k.At(i + 1))) << i;
    EXPECT_LT(Kwg::Children(k.At(i)), k.NumArcs());
  }
  EXPECT_TRUE(Kwg::IsEnd(k.At(k.NumArcs() - 1)));
}

TEST(Kwg, BytesRoundTrip) {
  Kwg k = Kwg::FromWords(SmallWords());
  std::string bytes = k.ToBytes();
  EXPECT_EQ(bytes.size(), 4 * k.NumArcs());
  // Little-endian: the low byte of arc 0 comes first.
  EXPECT_EQ(static_cast<unsigned char>(bytes[0]), k.At(0) & 0xFF);
  Kwg again = Kwg::FromBytes(bytes);
  EXPECT_EQ(again.ToBytes(), bytes);
  EXPECT_THROW(Kwg::FromBytes("abc"), std::runtime_error);
  EXPECT_THROW(Kwg::FromBytes(std::string(8, '\xFF')), std::runtime_error);
}

TEST(Kwg, MatchesTheLexicon) {
  std::vector<std::string> words = SmallWords();
  Kwg k = Kwg::FromWords(words);
  EXPECT_EQ(k.Words(), words);
  EXPECT_EQ(GaddagPaths(k), ExpectedPaths(words));
}

TEST(Kwg, ConvertsToTheSameGaddag) {
  std::vector<std::string> words = SmallWords();
  Gaddag direct = Gaddag::FromWords(words);
  Gaddag loaded = Kwg::FromWords(words).ToGaddag();
  EXPECT_EQ(loaded.NumWords(), direct.NumWords());
  EXPECT_EQ(loaded.NumNodes(), direct.NumNodes());
  EXPECT_EQ(loaded.NumEdges(), direct.NumEdges());
  for (const auto& w : words) EXPECT_TRUE(loaded.Contains(w)) << w;
}

// enable_small.kwg was written by wolges (buildlex kwg), the reference implementation.
TEST(Kwg, ReadsWolgesOutput) {
  Kwg k = Kwg::FromFile(std::string(KINGBBLE_FIXTURES) + "/enable_small.kwg");
  std::vector<std::string> words = SmallWords();
  EXPECT_EQ(k.Words(), words);
  EXPECT_EQ(GaddagPaths(k), ExpectedPaths(words));
  EXPECT_EQ(GaddagPaths(Kwg::FromWords(words)), GaddagPaths(k));
}

}  // namespace scrabble
