// Self-play: builds the GADDAG, plays N games between two bots and reports
// results and throughput. Bots swap seats every game, so each starts half of them.
//
//   kingbble_selfplay --lexicon lexica/ENABLE.txt --games 100 --seed 1 [--verbose] [--progress]
//   kingbble_selfplay --lexicon lexica/ENABLE.txt --leaves lexica/NWL23.klv2 --a equity --b greedy
//   kingbble_selfplay ... --a sim --b equity --sim-candidates 10 --sim-iterations 200 --threads 8
//   kingbble_selfplay ... --a equity+eg --b equity --endgame-time 5
//   kingbble_selfplay ... --a equity+peg+eg --b equity+eg --peg-bag 1 --peg-time 10
// A "+eg" suffix makes a bot solve the endgame once the bag is empty; "+peg"
// makes it solve the pre-endgame with 1..peg-bag tiles in the bag (the best
// --peg-candidates plays by static equity, within --peg-time seconds).
// "+apeg" plays the approximate pre-endgame with peg-bag + 1..6 tiles in the bag
// (--apeg-candidates plays, --apeg-samples deals, --apeg-plies endgame depth,
// within --apeg-time seconds, on --threads threads).
// With --results FILE every finished game is appended to FILE and a rerun skips
// the games already there; --status prints the totals of FILE and exits.
// Suffixes stack: "sim+peg+eg", "equity+apeg+peg+eg".
//   kingbble_selfplay ... --a risk --b sim --paired --risk-lambda 1 --risk-shrink 25
// "risk" and "riskwin" pick among the simulated candidates by mean - lambda *
// variance or by win % with 7-25 tiles in the bag (RiskParams): --risk-lambda
// scales the theoretical lambda, --risk-rest-sd sets the spread to come per
// root of a tile, --risk-shrink the variance shrinkage, --risk-relative uses
// the variance relative to the best candidate and --risk-iterations multiplies
// the iterations inside the window. --sim-plies sets both bots' playout depth
// and --a-plies bot a's alone. Result lines end with each bot's moves and
// seconds (", a M moves T s, b M moves T s"). --paired plays each deal twice, games 2k-1 and 2k,
// with the bots swapping seats, so luck of the draw cancels out in each pair.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "bot_factory.hpp"
#include "results_file.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"

using namespace scrabble;

namespace {

// SplitMix64: decorrelates per-game seeds so different --seed values share no games.
std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

// Totals over every finished game ("game N: S1 - S2, a in seat K" lines).
void PrintSummary(const ResultsFile& results, int games, const std::string& kind_a,
                  const std::string& kind_b) {
  long long total_score = 0, spread_a = 0;
  int wins[3] = {0, 0, 0};     // by seat: p1, p2, ties
  int wins_ab[3] = {0, 0, 0};  // by bot: a, b, ties
  int n = 0;
  for (const std::string& line : results.lines()) {
    int g = 0, s0 = 0, s1 = 0, seat = 0;
    if (std::sscanf(line.c_str(), "game %d: %d - %d, a in seat %d", &g, &s0, &s1, &seat) != 4 ||
        g > games)
      continue;
    ++n;
    total_score += s0 + s1;
    ++wins[s0 > s1 ? 0 : s1 > s0 ? 1 : 2];
    const int sa = seat == 1 ? s0 : s1, sb = seat == 1 ? s1 : s0;
    spread_a += sa - sb;
    ++wins_ab[sa > sb ? 0 : sb > sa ? 1 : 2];
  }
  std::cout << "games: " << n << " of " << games << ", wins p1/p2/ties: " << wins[0] << "/"
            << wins[1] << "/" << wins[2]
            << ", avg score per player: " << (n ? total_score / (2.0 * n) : 0) << "\n";
  std::cout << "a=" << kind_a << " vs b=" << kind_b << ": wins a/b/ties: " << wins_ab[0] << "/"
            << wins_ab[1] << "/" << wins_ab[2]
            << ", a win %: " << (n ? 100.0 * (wins_ab[0] + 0.5 * wins_ab[2]) / n : 0)
            << ", a avg spread: " << (n ? double(spread_a) / n : 0) << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, kind_a = "greedy", kind_b = "greedy";
  int games = 10;
  std::uint64_t seed = 1;
  bool verbose = false;
  bool progress = false;  // "progress: G/N" on stderr after each game
  std::string results_path;
  bool status_only = false;
  bool paired = false;
  int a_plies = 0;  // 0: as b
  SimParams sim;
  RiskParams risk;
  double endgame_time = 5;
  PegOptions peg;
  peg.params.candidates = 10;
  peg.params.time_limit_s = 10;
  peg.approx.time_limit_s = 10;
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
    } else if (a == "--a-plies") {
      a_plies = std::stoi(value());
    } else if (a == "--sim-plies") {
      sim.plies = std::stoi(value());
    } else if (a == "--sim-time") {
      sim.time_limit_s = std::stod(value());
    } else if (a == "--endgame-time") {
      endgame_time = std::stod(value());
    } else if (a == "--peg-bag") {
      peg.max_bag = std::stoi(value());
    } else if (a == "--peg-candidates") {
      peg.params.candidates = std::stoi(value());
    } else if (a == "--peg-time") {
      peg.params.time_limit_s = std::stod(value());
    } else if (a == "--apeg-candidates") {
      peg.approx.candidates = std::stoi(value());
    } else if (a == "--apeg-samples") {
      peg.approx.samples = std::stoi(value());
    } else if (a == "--apeg-plies") {
      peg.approx.endgame_plies = std::stoi(value());
    } else if (a == "--apeg-time") {
      peg.approx.time_limit_s = std::stod(value());
    } else if (a == "--threads") {
      sim.threads = std::stoi(value());
    } else if (a == "--games") {
      games = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--verbose") {
      verbose = true;
    } else if (a == "--progress") {
      progress = true;
    } else if (a == "--results") {
      results_path = value();
    } else if (a == "--status") {
      status_only = true;
    } else if (a == "--paired") {
      paired = true;
    } else if (a == "--risk-lambda") {
      risk.lambda_scale = std::stod(value());
    } else if (a == "--risk-shrink") {
      risk.shrink = std::stod(value());
    } else if (a == "--risk-relative") {
      risk.relative_variance = true;
    } else if (a == "--risk-iterations") {
      risk.iterations_factor = std::stoi(value());
    } else if (a == "--risk-rest-sd") {
      risk.win.rest_sd_per_sqrt_tile = std::stod(value());
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE [--leaves KLV] [--a BOT] [--b BOT] [--games N] [--seed S]"
                   " [--sim-candidates N] [--sim-iterations N] [--sim-plies N] [--a-plies N] "
                   "[--sim-time S]"
                   " [--threads N] [--endgame-time S]"
                   " [--peg-bag N] [--peg-candidates N] [--peg-time S] [--apeg-candidates N]"
                   " [--apeg-samples N] [--apeg-plies N] [--apeg-time S] [--verbose]"
                   " [--progress] [--results FILE [--status]] [--paired] [--risk-lambda L]"
                   " [--risk-rest-sd S] [--risk-shrink K] [--risk-relative]"
                   " [--risk-iterations F]\n";
      return 2;
    }
  }
  if (status_only) {
    PrintSummary(ResultsFile(results_path, true), games, kind_a, kind_b);
    return 0;
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
  SimParams a_sim = sim;
  if (a_plies > 0) a_sim.plies = a_plies;
  std::unique_ptr<Bot> bots[2] = {
      MakeBot(kind_a, gaddag, leaves.get(), a_sim, endgame_time, peg, risk),
      MakeBot(kind_b, gaddag, leaves.get(), sim, endgame_time, peg, risk)};

  long long turns = 0, generated = 0;
  double gen_s = 0;
  ResultsFile results(results_path);
  auto report = [&] {
    int done = 0;
    for (int g = 1; g <= games; ++g) done += results.Done(g);
    if (progress) std::cerr << "progress: " << done << "/" << games << std::endl;
  };
  report();
  for (int g = 0; g < games; ++g) {
    if (results.Done(g + 1)) continue;
    const int deal = paired ? g / 2 : g;
    Game game(MixSeed(MixSeed(seed) + static_cast<std::uint64_t>(deal)));
    const int seat_a = g % 2;  // seat of bot a in this game
    int moves[2] = {0, 0};     // by bot: a, b
    double seconds[2] = {0, 0};
    while (!game.over()) {
      const int side = game.to_move() == seat_a ? 0 : 1;
      Bot& bot = *bots[side];
      auto t = Clock::now();
      Move m = bot.Choose(game);
      const double spent = std::chrono::duration<double>(Clock::now() - t).count();
      gen_s += spent;
      seconds[side] += spent;
      ++moves[side];
      generated += static_cast<long long>(bot.last_num_moves());
      ++turns;
      if (verbose) {
        std::cout << "  p" << game.to_move() + 1 << " " << game.rack(game.to_move()).ToString()
                  << "  " << m.ToString() << "\n";
      }
      game.Apply(m);
    }
    const int s0 = game.score(0), s1 = game.score(1);
    char timing[96];
    std::snprintf(timing, sizeof timing, ", a %d moves %.2f s, b %d moves %.2f s", moves[0],
                  seconds[0], moves[1], seconds[1]);
    results.Append(g + 1, "game " + std::to_string(g + 1) + ": " + std::to_string(s0) + " - " +
                              std::to_string(s1) + ", a in seat " + std::to_string(seat_a + 1) +
                              timing);
    if (verbose) std::cout << "game " << g + 1 << ": " << s0 << " - " << s1 << "\n";
    report();
  }
  PrintSummary(results, games, kind_a, kind_b);
  std::cout << "turns: " << turns
            << ", avg moves per turn: " << (turns ? double(generated) / turns : 0)
            << ", positions/s: " << (gen_s > 0 ? turns / gen_s : 0)
            << ", moves/s: " << (gen_s > 0 ? generated / gen_s : 0) << "\n";
  return 0;
}
