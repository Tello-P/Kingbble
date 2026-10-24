#include "scrabble/features.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <set>
#include <string>

#include "scrabble/equity.hpp"
#include "scrabble/gaddag.hpp"

namespace scrabble {

namespace {

std::string Fixture(const std::string& name) { return std::string(KINGBBLE_FIXTURES) + "/" + name; }

}  // namespace

TEST(Features, ReplayTurnsNumbersMovesAndFlagsPhonies) {
  std::string error;
  auto game = ReadGcgFile(Fixture("sample.gcg"), &error);
  ASSERT_TRUE(game) << error;
  std::vector<ReplayedTurn> turns = ReplayTurns(*game, &error);
  ASSERT_EQ(turns.size(), 8u) << error;
  EXPECT_EQ(turns[0].bag_size, 86);
  EXPECT_TRUE(turns[0].known_rack);
  EXPECT_EQ(turns[0].rack.ToString(), "AERTXYZ");
  // RATE is on the board before bob's RATES: 4 tiles out of the bag.
  EXPECT_EQ(turns[1].bag_size, 82);
  EXPECT_FALSE(turns[1].board.IsEmpty(7, 4));
  EXPECT_EQ(turns[1].move.tiles, "....S");
  EXPECT_TRUE(turns[4].phony);  // (S)OX, withdrawn
  EXPECT_FALSE(turns[5].phony);
  EXPECT_FALSE(turns[7].known_rack);  // -3: unknown tiles
  EXPECT_EQ(turns[7].turn, 7);
}

TEST(Features, RankOptionsIsSortedAndFindsThePlayedMove) {
  Gaddag gaddag = Gaddag::FromWords({"AT", "TA", "EAT", "TEA", "ATE", "RATE", "TEAR", "RATES"});
  Leaves leaves = Leaves::FromFile(Fixture("leaves_small.klv2"));
  MoveGenerator gen(gaddag);
  Board board;
  Rack rack = *Rack::FromString("AERTXYZ");
  auto options = RankOptions(gen, board, rack, 86, UnseenTiles(board, rack), leaves);
  ASSERT_GT(options.size(), 3u);
  EXPECT_EQ(options.front().features[kFeatGap], 0.f);
  EXPECT_EQ(options.front().features[kFeatLogRank], 0.f);
  for (std::size_t i = 1; i < options.size(); ++i) {
    EXPECT_GE(options[i - 1].equity, options[i].equity);
    EXPECT_GE(options[i].features[kFeatGap], 0.f);
  }
  Move rate;
  rate.kind = Move::Kind::kPlace;
  rate.row = 7;
  rate.col = 4;
  rate.tiles = "RATE";
  const int i = FindOption(options, rate);
  ASSERT_GE(i, 0);
  EXPECT_EQ(options[i].features[kFeatTiles], 4.f);
  EXPECT_EQ(options[i].features[kFeatScore], 8.f);
  Move pass;
  const int p = FindOption(options, pass);
  ASSERT_GE(p, 0);
  EXPECT_EQ(options[p].features[kFeatPass], 1.f);
  Move exchange;
  exchange.kind = Move::Kind::kExchange;
  exchange.tiles = "ZX";
  const int x = FindOption(options, exchange);
  ASSERT_GE(x, 0);
  EXPECT_EQ(options[x].features[kFeatExchange], 1.f);
  Move phony = rate;
  phony.tiles = "TARE";
  EXPECT_EQ(FindOption(options, phony), -1);
}

TEST(Features, OneTilePlaysMatchInEitherDirection) {
  Move a;
  a.kind = Move::Kind::kPlace;
  a.row = 6;
  a.col = 7;
  a.tiles = "A.";
  Move b = a;
  b.dir = Direction::kVertical;
  b.tiles = "A";
  EXPECT_TRUE(SamePlay(a, b));
  b.col = 8;
  EXPECT_FALSE(SamePlay(a, b));
}

TEST(Features, NamesAreUniqueAndKeepTheBaseOrder) {
  const auto& names = MoveFeatureNames();
  std::set<std::string> seen(names.begin(), names.end());
  EXPECT_EQ(seen.size(), names.size());
  EXPECT_STREQ(names[kFeatGap], "gap");
  EXPECT_STREQ(names[kFeatPass], "pass");
  EXPECT_STREQ(names[kFeatLeaveCount0], "leave_n_blank");
  EXPECT_STREQ(names[kFeatLeaveCountLast], "leave_n_z");
  EXPECT_EQ(MoveFeatureIndex("opens_tw"), kFeatOpensTW);
  EXPECT_EQ(MoveFeatureIndex("nope"), -1);
}

TEST(Features, ReplayTurnsTracksTheScoreAndExchanges) {
  auto game = ReadGcgFile(Fixture("sample.gcg"));
  ASSERT_TRUE(game);
  std::vector<ReplayedTurn> turns = ReplayTurns(*game);
  ASSERT_EQ(turns.size(), 8u);
  EXPECT_EQ(turns[0].score_diff, 0);
  EXPECT_EQ(turns[1].score_diff, -8);
  EXPECT_EQ(turns[2].score_diff, 3);
  EXPECT_EQ(turns[5].score_diff, -7);  // after the phony came off
  EXPECT_TRUE(turns[5].prev_exchange);
  EXPECT_FALSE(turns[6].prev_exchange);
  EXPECT_FALSE(turns[7].prev_exchange);
}

TEST(Features, LexiconCountsSevenLetterAnagramsWithBlanks) {
  FeatureLexicon lex = FeatureLexicon::FromWords({"RETAINS", "NASTIER", "RETINAS", "TEA"});
  EXPECT_EQ(lex.SevenAnagrams(*Rack::FromString("AEINRST")), 3);
  EXPECT_EQ(lex.SevenAnagrams(*Rack::FromString("?EINRST")), 3);
  EXPECT_EQ(lex.SevenAnagrams(*Rack::FromString("??INRST")), 3);
  EXPECT_FALSE(lex.HasSeven(*Rack::FromString("AEINRSX")));
  EXPECT_FALSE(lex.HasSeven(*Rack::FromString("AET")));
  lex.SetInfo("TEA", {4.5f, 10, false});
  lex.MarkAlt("TEA");
  ASSERT_NE(lex.Find("TEA"), nullptr);
  EXPECT_TRUE(lex.Find("TEA")->alt);
  EXPECT_EQ(lex.Find("TEA")->zipf, 4.5f);
  EXPECT_EQ(lex.Find("EAT"), nullptr);
}

TEST(Features, MainWordReadsPlayedThroughLetters) {
  Board board;
  board.Place(7, 7, *TileFromChar('A'), false);
  board.Place(7, 8, *TileFromChar('T'), true);
  Move m;
  m.kind = Move::Kind::kPlace;
  m.row = 7;
  m.col = 6;
  m.tiles = "e..";
  EXPECT_EQ(MainWord(board, m), "EAT");
  EXPECT_EQ(MainWord(board, Move{}), "");
}

TEST(Features, FillComputesTheExtendedFeatures) {
  Gaddag gaddag = Gaddag::FromWords({"AT", "TA", "EAT", "TEA", "ATE", "RATE", "TEAR", "RATES"});
  Leaves leaves = Leaves::FromFile(Fixture("leaves_small.klv2"));
  FeatureLexicon lex = FeatureLexicon::FromWords({"RETAINS"});
  lex.SetInfo("RATE", {3.f, 2, true});
  MoveGenerator gen(gaddag);
  Board board;
  Rack rack = *Rack::FromString("AERTSXZ");
  auto options = RankOptions(gen, board, rack, 86, UnseenTiles(board, rack), leaves);
  FeatureComputer fc(leaves, &lex);
  TurnContext ctx{86, -50, 0, true, 2000};
  fc.Fill(board, rack, ctx, options, static_cast<int>(options.size()));
  Move rate;
  rate.kind = Move::Kind::kPlace;
  rate.row = 7;
  rate.col = 4;
  rate.tiles = "RATE";
  const int i = FindOption(options, rate);
  ASSERT_GE(i, 0);
  const auto& f = options[i].features;
  EXPECT_EQ(f[kFeatWordLength], 4.f);
  EXPECT_EQ(f[kFeatWordZipf], 3.f);
  EXPECT_FLOAT_EQ(f[kFeatWordPlays], std::log1p(2.f));
  EXPECT_EQ(f[kFeatWordAlt], 1.f);
  EXPECT_EQ(f[kFeatWordUnfamiliar], 0.f);
  EXPECT_LT(f[kFeatWordLogProb], 0.f);
  EXPECT_EQ(f[kFeatLeaveSize], 3.f);  // SXZ
  EXPECT_EQ(f[kFeatLeaveVowels], 0.f);
  EXPECT_EQ(f[kFeatLeaveConsonants], 3.f);
  EXPECT_EQ(f[kFeatLeaveS], 1.f);
  EXPECT_EQ(f[kFeatLeaveCount0 + *TileFromChar('X')], 1.f);
  EXPECT_GE(f[kFeatLeaveBingoProb], 0.f);
  EXPECT_LE(f[kFeatLeaveBingoProb], 1.f);
  EXPECT_EQ(f[kFeatOnDW], 1.f);  // 8E-8H covers the centre
  EXPECT_EQ(f[kFeatThrough], 0.f);
  EXPECT_EQ(f[kFeatCrossWords], 0.f);
  EXPECT_GT(f[kFeatBingoLanes], 0.f);  // the first move opens lanes
  EXPECT_EQ(f[kFeatOpensTW], 4.f);  // 8A and 8O by the row, 1H and 15H by the centre column
  EXPECT_EQ(f[kFeatHadBingo], 0.f);
  EXPECT_EQ(f[kFeatMissedBingo], 0.f);
  EXPECT_FLOAT_EQ(f[kFeatScoreDiff], -0.5f);
  EXPECT_EQ(f[kFeatPrevExchange], 1.f);
  EXPECT_FLOAT_EQ(f[kFeatRating], 1.f);
  EXPECT_FLOAT_EQ(f[kFeatGapRating], f[kFeatGap]);
  EXPECT_FLOAT_EQ(f[kFeatGapPhase], 0.f);  // nothing drawn yet
  EXPECT_FLOAT_EQ(f[kFeatScoreLead], f[kFeatScore] * std::tanh(-0.5f));
  // The best scorer has no option scoring more.
  int top = 0;
  for (std::size_t j = 0; j < options.size(); ++j)
    if (options[j].move.score > options[top].move.score) top = static_cast<int>(j);
  EXPECT_EQ(options[top].features[kFeatScoreRank], 0.f);
  // Exchanges and passes have no word.
  const int x = FindOption(options, Move{});
  EXPECT_EQ(options[x].features[kFeatWordLength], 0.f);
  EXPECT_EQ(options[x].features[kFeatLeaveSize], 7.f);
}

TEST(Features, LeaveBingoProbabilityIsExact) {
  Leaves leaves = Leaves::FromFile(Fixture("leaves_small.klv2"));
  FeatureLexicon lex = FeatureLexicon::FromWords({"RETAINS"});
  FeatureComputer fc(leaves, &lex);
  EXPECT_EQ(fc.LeaveBingoProbability(*Rack::FromString("AEINRST")), 1.0);
  EXPECT_EQ(fc.LeaveBingoProbability(*Rack::FromString("AEINRSX")), 0.0);
  // Exact without a blank on the leave: an A, 9 of the 94 tiles left.
  EXPECT_NEAR(fc.LeaveBingoProbability(*Rack::FromString("EINRST")), 9.0 / 94, 1e-6);
  // AEINRST from a full bag: 9 * 12 * 9 * 6 * 6 * 4 * 6 ways out of C(100, 7).
  EXPECT_NEAR(fc.LeaveBingoProbability(Rack{}), 9.0 * 12 * 9 * 6 * 6 * 4 * 6 / 16007560800.0, 1e-12);
  // A blank on the leave: an A or a T, 15 of the 94 tiles left.
  EXPECT_NEAR(fc.LeaveBingoProbability(*Rack::FromString("?EINRS")), 15.0 / 94, 1e-6);
  EXPECT_NEAR(fc.LeaveBingoProbability(*Rack::FromString("?EINRST")), 1.0, 1e-6);
  EXPECT_NEAR(fc.LeaveBingoProbability(*Rack::FromString("??EINRS")), 1.0, 1e-6);
  EXPECT_EQ(FeatureComputer(leaves, nullptr).LeaveBingoProbability(*Rack::FromString("EINRST")), 0);
}

}  // namespace scrabble
