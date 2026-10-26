// Risk diagnostic: how often RiskBot picks another move than SimBot, and how
// much simulated mean it gives up when it does. Plays games with the equity bot
// up to the risk window (--max-bag tiles in the bag), then with a non-pruning
// SimBot until --min-bag; on every turn in between, scores the same simulation
// with each risk setting and compares its pick with the best mean.
//   kingbble_risk_diag --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 --games 100
//       [--seed 1] [--sim-candidates 8] [--sim-iterations 50] [--threads 3] [--progress]
// Prints one line per setting: turns in the window, turns where the risk utility
// applied (the game not decided), % of turns with another move, the mean given
// up per changed move and per turn, and the % of changed moves by bag size.

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "scrabble/equity_bot.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/risk_bot.hpp"
#include "scrabble/sim_bot.hpp"

using namespace scrabble;

namespace {

std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

struct Setting {
  std::string name;
  RiskParams params;
  long long turns = 0, active = 0, changed = 0;
  double lost = 0;  // simulated mean given up, summed over changed moves
  long long changed_by_band[3] = {0, 0, 0}, turns_by_band[3] = {0, 0, 0};
};

int Band(int bag) { return bag <= 12 ? 0 : bag <= 18 ? 1 : 2; }

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path;
  int games = 100;
  std::uint64_t seed = 1;
  bool progress = false;
  SimParams sim;
  sim.candidates = 8;
  sim.max_iterations = 50;
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
    } else if (a == "--games") {
      games = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--sim-candidates") {
      sim.candidates = std::stoi(value());
    } else if (a == "--sim-iterations") {
      sim.max_iterations = std::stoi(value());
    } else if (a == "--threads") {
      sim.threads = std::stoi(value());
    } else if (a == "--progress") {
      progress = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE --leaves KLV [--games N] [--seed S] [--sim-candidates N]"
                   " [--sim-iterations N] [--threads N] [--progress]\n";
      return 2;
    }
  }
  if (lexicon.empty() || leaves_path.empty()) {
    std::cerr << "--lexicon and --leaves are required\n";
    return 2;
  }
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  sim.seed = seed;
  sim.prune_z = 1e9;
  EquityBot equity(gaddag, leaves);
  SimBot simbot(gaddag, leaves, sim);

  std::vector<Setting> settings;
  auto add = [&](const std::string& name, auto&& tweak) {
    Setting s;
    s.name = name;
    tweak(s.params);
    settings.push_back(s);
  };
  for (double scale : {0.5, 1.0, 2.0, 4.0})
    add("risk l" + std::to_string(scale).substr(0, 3),
        [&](RiskParams& p) { p.lambda_scale = scale; });
  add("risk l1.0 no shrink", [](RiskParams& p) { p.shrink = 0; });
  add("risk l1.0 relative", [](RiskParams& p) { p.relative_variance = true; });
  add("risk l1.0 decided too", [](RiskParams& p) { p.decided = 0; });
  add("riskwin", [](RiskParams& p) { p.mode = RiskMode::kWinProb; });
  const int min_bag = settings.front().params.min_bag, max_bag = settings.front().params.max_bag;

  for (int g = 0; g < games; ++g) {
    Game game(MixSeed(MixSeed(seed) + static_cast<std::uint64_t>(g)));
    while (!game.over() && game.bag_size() > max_bag) game.Apply(equity.Choose(game));
    while (!game.over() && game.bag_size() >= min_bag) {
      Move m = simbot.Choose(game);
      const std::vector<SimResult>& sims = simbot.last_results();
      const double best_mean = sims.front().mean;
      for (Setting& s : settings) {
        bool active = false;
        std::vector<RiskResult> r = ScoreRisk(game, sims, s.params, &active);
        const int band = Band(game.bag_size());
        ++s.turns;
        ++s.turns_by_band[band];
        s.active += active;
        if (!(r.front().sim.move == m)) {
          ++s.changed;
          ++s.changed_by_band[band];
          s.lost += best_mean - r.front().sim.mean;
        }
      }
      game.Apply(m);
    }
    if (progress) std::cerr << "progress: " << g + 1 << "/" << games << std::endl;
  }

  std::cout << std::fixed << std::setprecision(1);
  std::cout << "setting\tturns\tactive %\tchanged %\tlost/changed\tlost/turn\tchanged % by bag "
               "7-12/13-18/19-25\n";
  for (const Setting& s : settings) {
    auto pct = [](long long a, long long b) { return b ? 100.0 * a / b : 0.0; };
    std::cout << s.name << "\t" << s.turns << "\t" << pct(s.active, s.turns) << "\t"
              << pct(s.changed, s.turns) << "\t" << (s.changed ? s.lost / s.changed : 0) << "\t"
              << (s.turns ? s.lost / s.turns : 0) << "\t"
              << pct(s.changed_by_band[0], s.turns_by_band[0]) << "/"
              << pct(s.changed_by_band[1], s.turns_by_band[1]) << "/"
              << pct(s.changed_by_band[2], s.turns_by_band[2]) << "\n";
  }
  return 0;
}
