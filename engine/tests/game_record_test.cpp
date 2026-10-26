#include "../apps/game_record.hpp"

#include <gtest/gtest.h>

#include <string>

#include "../apps/bot_factory.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

}  // namespace

// A self-play game between the equity bot and the human model's bot, saved as
// GCG, parses back and replays with the same scores.
TEST(GameRecord, SelfPlayGameReplays) {
  Gaddag gaddag = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  HumanModelParams params;
  HumanBotOptions human;
  human.params = &params;
  SimParams sim;
  std::unique_ptr<Bot> bots[2] = {MakeBot("equity", gaddag, &leaves, sim, 0, {}, {}, human),
                                  MakeBot("human", gaddag, &leaves, sim, 0, {}, {}, human)};
  for (std::uint64_t seed = 1; seed <= 3; ++seed) {
    Game game(seed);
    GameRecord record({"p1", "p2"}, {"equity", "human"}, "enable_small", "test");
    while (!game.over()) {
      const Move m = bots[game.to_move()]->Choose(game);
      record.Add(game, m);
      game.Apply(m);
    }
    record.Finish(game);
    std::string error;
    auto parsed = ParseGcg(WriteGcg(record.gcg()), &error);
    ASSERT_TRUE(parsed) << error;
    GcgReplay replay = ReplayGcg(*parsed);
    ASSERT_TRUE(replay.ok) << replay.error;
    EXPECT_EQ(replay.scores[0], game.score(0));
    EXPECT_EQ(replay.scores[1], game.score(1));
  }
}

}  // namespace scrabble
