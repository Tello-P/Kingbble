#include "scrabble/gcg.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

#include "scrabble/game.hpp"
#include "scrabble/greedy_bot.hpp"
#include "scrabble/movegen.hpp"

namespace scrabble {

namespace {

std::string Fixture(const std::string& name) { return std::string(KINGBBLE_FIXTURES) + "/" + name; }

GcgGame Sample() {
  std::string error;
  auto game = ReadGcgFile(Fixture("sample.gcg"), &error);
  EXPECT_TRUE(game) << error;
  return game ? *game : GcgGame{};
}

std::string ReplayError(std::string_view text) {
  std::string error;
  auto game = ParseGcg(text, &error);
  if (!game) return "parse: " + error;
  return ReplayGcg(*game).error;
}

const char* kHeader = "#player1 a\n#player2 b\n";

}  // namespace

TEST(Gcg, ParsesHeaderAndMoveKinds) {
  GcgGame g = Sample();
  EXPECT_EQ(g.nicknames[0], "alice");
  EXPECT_EQ(g.names[0], "Alice Smith");
  EXPECT_EQ(g.nicknames[1], "bob");
  EXPECT_EQ(g.names[1], "Bob Jones");
  EXPECT_EQ(g.lexicon, "ENABLE");
  EXPECT_EQ(g.id, "kingbble sample-1");
  using K = GcgEvent::Kind;
  std::vector<K> kinds;
  for (const auto& ev : g.events) kinds.push_back(ev.kind);
  EXPECT_EQ(kinds, (std::vector<K>{K::kPlace, K::kPlace, K::kPlace, K::kExchange, K::kPlace,
                                   K::kPhonyWithdrawn, K::kPlace, K::kChallengeBonus, K::kPass,
                                   K::kExchange, K::kTimePenalty, K::kEndRackPoints}));

  const GcgEvent& first = g.events[0];
  EXPECT_EQ(first.player, 0);
  EXPECT_EQ(first.rack, "AERTXYZ");
  EXPECT_EQ(first.move.row, 7);
  EXPECT_EQ(first.move.col, 4);
  EXPECT_EQ(first.move.dir, Direction::kHorizontal);
  EXPECT_EQ(first.move.tiles, "RATE");
  EXPECT_EQ(first.score, 8);

  const GcgEvent& vertical = g.events[2];
  EXPECT_EQ(vertical.move.dir, Direction::kVertical);
  EXPECT_EQ(vertical.move.row, 7);
  EXPECT_EQ(vertical.move.col, 6);

  EXPECT_EQ(g.events[3].move.tiles, "AEIOU");
  EXPECT_EQ(g.events[3].exchanged, 5);
  EXPECT_EQ(g.events[4].move.tiles, ".OX");
  EXPECT_EQ(g.events[9].move.tiles, "");
  EXPECT_EQ(g.events[9].exchanged, 3);
  EXPECT_EQ(g.events[11].rack, "AOX");
}

TEST(Gcg, ReplayVerifiesScoresAndBuildsBoard) {
  GcgReplay r = ReplayGcg(Sample());
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(r.scores[0], 2);
  EXPECT_EQ(r.scores[1], 32);
  ASSERT_EQ(r.moves.size(), 12u);
  EXPECT_EQ(r.moves[1].tiles, "....S");
  EXPECT_EQ(r.moves[2].tiles, ".IE");
  // The withdrawn SOX is gone; the blank of RaT stays.
  EXPECT_TRUE(r.board.IsEmpty(8, 8));
  EXPECT_TRUE(r.board.IsEmpty(9, 8));
  EXPECT_EQ(r.board.At(8, 4), 1 | Board::kBlankBit);
  EXPECT_EQ(r.board.At(7, 8), *TileFromChar('S'));
  EXPECT_TRUE(r.boards_before[0].IsEmpty(kCenter, kCenter));
  EXPECT_FALSE(r.boards_before[1].IsEmpty(kCenter, kCenter));
}

TEST(Gcg, WriteRoundTrips) {
  GcgGame g = Sample();
  std::string text = WriteGcg(g);
  std::string error;
  auto again = ParseGcg(text, &error);
  ASSERT_TRUE(again) << error;
  EXPECT_EQ(again->nicknames, g.nicknames);
  EXPECT_EQ(again->names, g.names);
  ASSERT_EQ(again->events.size(), g.events.size());
  for (std::size_t i = 0; i < g.events.size(); ++i) {
    EXPECT_EQ(again->events[i].kind, g.events[i].kind) << i;
    EXPECT_EQ(again->events[i].move, g.events[i].move) << i;
    EXPECT_EQ(again->events[i].score, g.events[i].score) << i;
    EXPECT_EQ(again->events[i].cumulative, g.events[i].cumulative) << i;
  }
  EXPECT_EQ(WriteGcg(*again), text);
}

TEST(Gcg, AssignsPlayersWithoutPragmas) {
  auto g = ParseGcg(">x: AERT 8E RATE +8 8\n>y: S 8I S +5 5\n>x: ? - +0 8\n");
  ASSERT_TRUE(g);
  EXPECT_EQ(g->nicknames[0], "x");
  EXPECT_EQ(g->nicknames[1], "y");
  EXPECT_EQ(g->events[2].player, 0);
}

// #71: Woogles writes these with an empty rack once the player has gone out.
TEST(Gcg, ReadsTimeAndChallengeLinesWithAnEmptyRack) {
  const std::string text = std::string(kHeader) +
                           ">a: AB 8H AB +8 8\n"
                           ">a:  (time) -10 -2\n"
                           ">b:  (challenge) +5 5\n";
  std::string error;
  auto game = ParseGcg(text, &error);
  ASSERT_TRUE(game) << error;
  ASSERT_EQ(game->events.size(), 3u);
  EXPECT_EQ(game->events[1].kind, GcgEvent::Kind::kTimePenalty);
  EXPECT_EQ(game->events[1].rack, "");
  EXPECT_EQ(game->events[1].score, -10);
  EXPECT_EQ(game->events[2].kind, GcgEvent::Kind::kChallengeBonus);
  EXPECT_EQ(game->events[2].player, 1);
  GcgReplay r = ReplayGcg(*game);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(r.scores[0], -2);
  EXPECT_EQ(r.scores[1], 5);
  EXPECT_TRUE(ParseGcg(WriteGcg(*game)));  // written back the same way
}

TEST(Gcg, RejectsMalformedLines) {
  std::string error;
  EXPECT_FALSE(ParseGcg(std::string(kHeader) + ">c: A 8H A +1 1\n", &error));
  EXPECT_NE(error.find("unknown player"), std::string::npos);
  EXPECT_FALSE(ParseGcg(std::string(kHeader) + ">a: AB 16A AB +4 4\n", &error));
  EXPECT_NE(error.find("line 3"), std::string::npos);
  EXPECT_FALSE(ParseGcg(std::string(kHeader) + ">a: AB 8H AB +4\n", &error));
  EXPECT_FALSE(ParseGcg(std::string(kHeader) + ">a: AB 8H A1 +4 4\n", &error));
  EXPECT_FALSE(ParseGcg(std::string(kHeader) + ">a: AB -8 +0 0\n", &error));
}

TEST(Gcg, ReplayCatchesInconsistencies) {
  const std::string h = kHeader;
  EXPECT_EQ(ReplayError(h + ">a: AERT 8E RATE +8 8\n"), "");
  EXPECT_NE(ReplayError(h + ">a: AERT 8E RATE +9 9\n").find("expected 8"), std::string::npos);
  EXPECT_NE(ReplayError(h + ">a: AERT 8E RATE +8 7\n").find("cumulative"), std::string::npos);
  EXPECT_NE(ReplayError(h + ">a: AEX 8E RATE +8 8\n").find("not on rack"), std::string::npos);
  EXPECT_NE(ReplayError(h + ">a: AERT 8E RATE +8 8\n>b: S 8E ROTES +5 5\n").find("match"),
            std::string::npos);
  EXPECT_NE(ReplayError(h + ">a: AERT 8M RATE +8 8\n").find("fit"), std::string::npos);
  EXPECT_NE(ReplayError(h + ">a: AERT 8E RATE +8 8\n>b: S -- -8 -8\n").find("withdrawal"),
            std::string::npos);
  EXPECT_NE(ReplayError(h + ">a: (AX) +16 16\n").find("end rack"), std::string::npos);
}

TEST(Gcg, SelfPlayGamesRoundTripThroughGcg) {
  std::ifstream in(Fixture("enable_small.txt"));
  std::vector<std::string> words;
  for (std::string w; in >> w;) words.push_back(w);
  Gaddag gaddag = Gaddag::FromWords(words);
  GreedyBot bot(gaddag);
  MoveGenerator gen(gaddag);
  for (std::uint64_t seed = 1; seed <= 5; ++seed) {
    Game game(seed);
    GcgGame record;
    record.nicknames = {"p1", "p2"};
    while (!game.over()) {
      const int p = game.to_move();
      Move m = bot.Choose(game);
      GcgEvent ev;
      ev.player = p;
      ev.rack = game.rack(p).ToString();
      ev.move = m;
      ev.kind = m.kind == Move::Kind::kPlace      ? GcgEvent::Kind::kPlace
                : m.kind == Move::Kind::kExchange ? GcgEvent::Kind::kExchange
                                                  : GcgEvent::Kind::kPass;
      ev.exchanged = m.kind == Move::Kind::kExchange ? static_cast<int>(m.tiles.size()) : 0;
      const int before = game.score(p);
      game.Apply(m);
      // End-of-game adjustments are folded into the last move by Game; keep only the move.
      ev.score = m.kind == Move::Kind::kPlace ? m.score : 0;
      ev.cumulative = before + ev.score;
      record.events.push_back(ev);
    }
    std::string error;
    auto parsed = ParseGcg(WriteGcg(record), &error);
    ASSERT_TRUE(parsed) << error;
    GcgReplay r = ReplayGcg(*parsed);
    ASSERT_TRUE(r.ok) << "seed " << seed << ": " << r.error;
    for (std::size_t i = 0; i < r.moves.size(); ++i) {
      if (r.moves[i].kind != Move::Kind::kPlace) continue;
      auto moves = gen.Generate(r.boards_before[i], *Rack::FromString(parsed->events[i].rack));
      auto it = std::find_if(moves.begin(), moves.end(), [&](const Move& m) {
        return m.row == r.moves[i].row && m.col == r.moves[i].col && m.tiles == r.moves[i].tiles &&
               (m.dir == r.moves[i].dir || m.TilesPlayed() == 1);
      });
      EXPECT_NE(it, moves.end()) << "seed " << seed << " move " << i;
    }
  }
}

}  // namespace scrabble
