// Self-play: builds the GADDAG, plays N games between two bots and reports
// results and throughput. Bots swap seats every game, so each starts half of them.
//
//   kingbble_selfplay --lexicon lexica/ENABLE.txt --games 100 --seed 1 [--verbose]
//   kingbble_selfplay --lexicon lexica/ENABLE.txt --leaves lexica/NWL23.klv2 --a equity --b greedy
//   kingbble_selfplay ... --a sim --b equity --sim-candidates 10 --sim-iterations 200 --threads 8
//   kingbble_selfplay ... --a equity+eg --b equity --endgame-time 5
// A "+eg" suffix makes a bot solve the endgame once the bag is empty.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "scrabble/endgame_bot.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/greedy_bot.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/sim_bot.hpp"

using namespace scrabble;

namespace {

// SplitMix64: decorrelates per-game seeds so different --seed values share no games.
std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

std::unique_ptr<Bot> MakeBot(const std::string& kind, const Gaddag& gaddag, const Leaves* leaves,
                             const SimParams& sim, double endgame_time) {
  const std::string suffix = "+eg";
  if (kind.size() > suffix.size() && kind.ends_with(suffix)) {
    return std::make_unique<EndgameBot>(
        MakeBot(kind.substr(0, kind.size() - suffix.size()), gaddag, leaves, sim, endgame_time),
        gaddag, endgame_time);
  }
  if (kind == "greedy") return std::make_unique<GreedyBot>(gaddag);
  if ((kind == "equity" || kind == "sim") && !leaves) {
    std::cerr << "bot '" << kind << "' needs --leaves\n";
    std::exit(2);
  }
  if (kind == "equity") return std::make_unique<EquityBot>(gaddag, *leaves);
  if (kind == "sim") return std::make_unique<SimBot>(gaddag, *leaves, sim);
  std::cerr << "unknown bot: " << kind << " (greedy, equity, sim, optionally with +eg)\n";
  std::exit(2);
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, kind_a = "greedy", kind_b = "greedy";
  int games = 10;
  std::uint64_t seed = 1;
  bool verbose = false;
  SimParams sim;
  double endgame_time = 5;
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
    } else if (a == "--leaves") {
      leaves_path = value();
    } else if (a == "--a") {
      kind_a = value();
    } else if (a == "--b") {
      kind_b = value();
    } else if (a == "--sim-candidates") {
      sim.candidates = std::stoi(value());
    } else if (a == "--sim-iterations") {
      sim.max_iterations = std::stoi(value());
    } else if (a == "--endgame-time") {
      endgame_time = std::stod(value());
    } else if (a == "--threads") {
      sim.threads = std::stoi(value());
    } else if (a == "--games") {
      games = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--verbose") {
      verbose = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE [--leaves KLV] [--a BOT] [--b BOT] [--games N] [--seed S]"
                   " [--sim-candidates N] [--sim-iterations N] [--threads N] [--endgame-time S]"
                   " [--verbose]\n";
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

  std::unique_ptr<Leaves> leaves;
  if (!leaves_path.empty()) {
    leaves = std::make_unique<Leaves>(Leaves::FromFile(leaves_path));
    std::cout << "leaves: " << leaves->Size() << "\n";
  }
  sim.seed = seed;
  std::unique_ptr<Bot> bots[2] = {MakeBot(kind_a, gaddag, leaves.get(), sim, endgame_time),
                                  MakeBot(kind_b, gaddag, leaves.get(), sim, endgame_time)};

  long long turns = 0, generated = 0, total_score = 0, spread_a = 0;
  int wins[3] = {0, 0, 0};     // by seat: p1, p2, ties
  int wins_ab[3] = {0, 0, 0};  // by bot: a, b, ties
  double gen_s = 0;
  for (int g = 0; g < games; ++g) {
    Game game(MixSeed(MixSeed(seed) + static_cast<std::uint64_t>(g)));
    const int seat_a = g % 2;  // seat of bot a in this game
    while (!game.over()) {
      Bot& bot = *bots[game.to_move() == seat_a ? 0 : 1];
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
    int sa = game.score(seat_a), sb = game.score(1 - seat_a);
    spread_a += sa - sb;
    ++wins_ab[sa > sb ? 0 : sb > sa ? 1 : 2];
    if (verbose) std::cout << "game " << g + 1 << ": " << s0 << " - " << s1 << "\n";
  }
  std::cout << "games: " << games << ", wins p1/p2/ties: " << wins[0] << "/" << wins[1] << "/"
            << wins[2] << ", avg score per player: " << (games ? total_score / (2.0 * games) : 0)
            << "\n";
  std::cout << "a=" << kind_a << " vs b=" << kind_b << ": wins a/b/ties: " << wins_ab[0] << "/"
            << wins_ab[1] << "/" << wins_ab[2]
            << ", a win %: " << (games ? 100.0 * (wins_ab[0] + 0.5 * wins_ab[2]) / games : 0)
            << ", a avg spread: " << (games ? double(spread_a) / games : 0) << "\n";
  std::cout << "turns: " << turns
            << ", avg moves per turn: " << (turns ? double(generated) / turns : 0)
            << ", positions/s: " << (gen_s > 0 ? turns / gen_s : 0)
            << ", moves/s: " << (gen_s > 0 ? generated / gen_s : 0) << "\n";
  return 0;
}
