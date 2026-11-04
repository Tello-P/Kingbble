// Reproducible tournaments: with per-game seeds and fixed iterations a game's
// moves depend only on the run's seed and the game's index, with and without
// opponent-rack inference.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../apps/bot_factory.hpp"
#include "../apps/run_seeds.hpp"

namespace scrabble {

namespace {

const std::string kFixtures = KINGBBLE_FIXTURES;

struct Fixture {
  Gaddag gaddag = Gaddag::FromFile(kFixtures + "/enable_small.txt");
  Leaves leaves = Leaves::FromFile(kFixtures + "/leaves_small.klv2");
  HumanModelParams human_params;
};

Fixture& Shared() {
  static Fixture f;
  return f;
}

SimParams FastSim(int threads) {
  SimParams sim;
  sim.candidates = 4;
  sim.max_iterations = 12;
  sim.min_iterations = 8;
  sim.batch = 4;
  sim.threads = threads;
  return sim;
}

BotSetup Make(const std::string& kind, int threads) {
  Fixture& f = Shared();
  HumanBotOptions human;
  human.params = &f.human_params;
  PegOptions peg;
  peg.endgame_plies = 3;
  peg.endgame_threads = 1;
  InferenceOptions inference;
  inference.filter.particles = 64;
  inference.filter.exact_limit = 200;
  return MakeBotSetup(kind, f.gaddag, &f.leaves, FastSim(threads), 0, peg, {}, human, inference);
}

// Game `g` of a paired run as selfplay plays it with --per-game-seeds: every
// move as text, then the final scores.
std::vector<std::string> PlayGame(BotSetup (&setups)[2], std::uint64_t seed, int g) {
  const int deal = g / 2, seat_a = g % 2;
  Game game(DealSeed(seed, deal));
  for (int side = 0; side < 2; ++side) {
    const int seat = side == 0 ? seat_a : 1 - seat_a;
    setups[side].NewGame(game, seat, StreamSeed(seed, deal, seat));
  }
  std::vector<std::string> moves;
  while (!game.over()) {
    const int side = game.to_move() == seat_a ? 0 : 1;
    const Move m = setups[side].bot->Choose(game);
    moves.push_back(m.ToString());
    for (BotSetup& s : setups) s.BeforeMove(game, m);
    game.Apply(m);
    for (BotSetup& s : setups) s.AfterMove(game);
  }
  moves.push_back(std::to_string(game.score(0)) + "-" + std::to_string(game.score(1)));
  return moves;
}

std::vector<std::vector<std::string>> PlayRun(const std::string& a, const std::string& b,
                                              std::uint64_t seed, int first, int games,
                                              int threads = 1) {
  BotSetup setups[2] = {Make(a, threads), Make(b, threads)};
  std::vector<std::vector<std::string>> out;
  for (int g = first; g < first + games; ++g) out.push_back(PlayGame(setups, seed, g));
  return out;
}

}  // namespace

TEST(Determinism, FactoryBuildsTrackers) {
  EXPECT_EQ(Make("sim", 1).tracker, nullptr);
  EXPECT_EQ(Make("sim+inf:none", 1).tracker, nullptr);
  EXPECT_NE(Make("sim+inf:none", 1).sim, nullptr);
  for (const std::string kind : {"sim+inf:rf", "sim+inf:rangefinder", "sim+inf:human"}) {
    BotSetup s = Make(kind, 1);
    EXPECT_NE(s.tracker, nullptr) << kind;
    EXPECT_NE(s.sim, nullptr) << kind;
  }
  BotSetup eg = Make("sim+inf:rf+eg", 1);
  EXPECT_NE(eg.tracker, nullptr);
  EXPECT_NE(dynamic_cast<EndgameBot*>(eg.bot.get()), nullptr);
}

TEST(Determinism, SameSeedSameGames) {
  for (const auto& [a, b] : std::vector<std::pair<std::string, std::string>>{
           {"equity", "greedy"}, {"sim", "equity"}, {"sim+inf:rf", "sim"}}) {
    const auto first = PlayRun(a, b, 7, 0, 2);
    EXPECT_EQ(first, PlayRun(a, b, 7, 0, 2)) << a << " vs " << b;
  }
}

TEST(Determinism, DifferentSeedsDifferentGames) {
  EXPECT_NE(PlayRun("equity", "greedy", 7, 0, 2), PlayRun("equity", "greedy", 8, 0, 2));
}

// A resumed run plays the games it skipped exactly as an uninterrupted one.
TEST(Determinism, GamesDoNotDependOnEarlierGames) {
  for (const std::string a : {"sim", "sim+inf:rf", "sim+inf:human"}) {
    const auto all = PlayRun(a, "sim", 3, 0, 3);
    const auto resumed = PlayRun(a, "sim", 3, 2, 1);
    EXPECT_EQ(all[2], resumed[0]) << a;
  }
}

TEST(Determinism, ThreadsDoNotChangeGames) {
  EXPECT_EQ(PlayRun("sim+inf:rf", "sim", 5, 0, 1, 1), PlayRun("sim+inf:rf", "sim", 5, 0, 1, 2));
}

TEST(Determinism, EndgameByDepthIsReproducible) {
  EXPECT_EQ(PlayRun("equity+eg", "equity", 11, 0, 2), PlayRun("equity+eg", "equity", 11, 0, 2));
}

}  // namespace scrabble
