// Greedy self-play: builds the GADDAG, plays N games and reports throughput.
//
//   kingbble_selfplay --lexicon lexica/ENABLE.txt --games 100 --seed 1 [--verbose]

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/greedy_bot.hpp"

using namespace scrabble;

namespace {

// SplitMix64: decorrelates per-game seeds so different --seed values share no games.
std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon;
  int games = 10;
  std::uint64_t seed = 1;
  bool verbose = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << a << "\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--lexicon") {
      lexicon = value();
    } else if (a == "--games") {
      games = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--verbose") {
      verbose = true;
    } else {
      std::cerr << "usage: " << argv[0] << " --lexicon FILE [--games N] [--seed S] [--verbose]\n";
      return 2;
    }
  }
  if (lexicon.empty()) {
    std::cerr << "--lexicon is required\n";
    return 2;
  }

  using Clock = std::chrono::steady_clock;
  auto t0 = Clock::now();
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  double build_s = std::chrono::duration<double>(Clock::now() - t0).count();
  std::cout << "lexicon: " << gaddag.NumWords() << " words, " << gaddag.NumNodes() << " nodes, "
            << gaddag.NumEdges() << " edges, built in " << build_s << " s\n";

  GreedyBot bot(gaddag);
  long long turns = 0, generated = 0, total_score = 0;
  int wins[3] = {0, 0, 0};
  double gen_s = 0;
  for (int g = 0; g < games; ++g) {
    Game game(MixSeed(MixSeed(seed) + static_cast<std::uint64_t>(g)));
    while (!game.over()) {
      auto t = Clock::now();
      Move m = bot.Choose(game);
      gen_s += std::chrono::duration<double>(Clock::now() - t).count();
      generated += static_cast<long long>(bot.last_num_moves());
      ++turns;
      if (verbose) {
        std::cout << "  p" << game.to_move() + 1 << " " << game.rack(game.to_move()).ToString()
                  << "  " << m.ToString() << "\n";
      }
      game.Apply(m);
    }
    int s0 = game.score(0), s1 = game.score(1);
    total_score += s0 + s1;
    ++wins[s0 > s1 ? 0 : s1 > s0 ? 1 : 2];
    if (verbose) std::cout << "game " << g + 1 << ": " << s0 << " - " << s1 << "\n";
  }
  std::cout << "games: " << games << ", wins p1/p2/ties: " << wins[0] << "/" << wins[1] << "/"
            << wins[2] << ", avg score per player: " << (games ? total_score / (2.0 * games) : 0)
            << "\n";
  std::cout << "turns: " << turns
            << ", avg moves per turn: " << (turns ? double(generated) / turns : 0)
            << ", positions/s: " << (gen_s > 0 ? turns / gen_s : 0)
            << ", moves/s: " << (gen_s > 0 ? generated / gen_s : 0) << "\n";
  return 0;
}
