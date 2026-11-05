// Counts for the win table: plays self-play games and, for every position,
// counts who won by the mover's spread and the tiles they cannot see. Writes lines
// "unseen spread points games" (spread rounded to a table step, clamped; ties count
// half a point). scripts/fit_win_table.py fits the table from these files.
//
//   kingbble_winstats --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2
//       --games 100000 --seed 1 --out stats.txt [--bot equity] [--progress]

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "bot_factory.hpp"
#include "scrabble/win_table.hpp"

using namespace scrabble;

namespace {

constexpr int kSpreads = 2 * WinTable::kMaxSpread / WinTable::kSpreadStep + 1;

std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

struct Seen {
  int unseen;
  int cell;
  int mover;
};

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, out_path, bot_kind = "equity";
  int games = 1000;
  std::uint64_t seed = 1;
  bool progress = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
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
    } else if (a == "--out") {
      out_path = value();
    } else if (a == "--bot") {
      bot_kind = value();
    } else if (a == "--games") {
      games = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--progress") {
      progress = true;
    } else {
      std::cerr << "usage: " << argv[0] << " --lexicon FILE --leaves KLV --out FILE [--games N]"
                << " [--seed S] [--bot KIND] [--progress]\n";
      return 2;
    }
  }
  if (lexicon.empty() || leaves_path.empty() || out_path.empty()) {
    std::cerr << "--lexicon, --leaves and --out are required\n";
    return 2;
  }

  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  SimParams sim;
  sim.seed = seed;
  std::unique_ptr<Bot> bot = MakeBot(bot_kind, gaddag, &leaves, sim, 1, PegOptions{});
  std::vector<double> points((WinTable::kMaxUnseen + 1) * kSpreads, 0);
  std::vector<std::int64_t> counts(points.size(), 0);

  for (int g = 0; g < games; ++g) {
    Game game(MixSeed(MixSeed(seed) + static_cast<std::uint64_t>(g)));
    std::vector<Seen> seen;
    while (!game.over()) {
      const int mover = game.to_move();
      const int spread = game.score(mover) - game.score(1 - mover);
      const int clamped = std::clamp(spread, -WinTable::kMaxSpread, WinTable::kMaxSpread);
      const int cell = static_cast<int>(
          std::lround((clamped + WinTable::kMaxSpread) / double(WinTable::kSpreadStep)));
      const int unseen = game.bag_size() + game.rack(1 - mover).Size();
      seen.push_back({std::min(unseen, WinTable::kMaxUnseen), cell, mover});
      game.Apply(bot->Choose(game));
    }
    const int final_spread = game.score(0) - game.score(1);
    for (const Seen& s : seen) {
      const int mover_spread = s.mover == 0 ? final_spread : -final_spread;
      const std::size_t i = static_cast<std::size_t>(s.unseen) * kSpreads + s.cell;
      points[i] += mover_spread > 0 ? 1 : mover_spread == 0 ? 0.5 : 0;
      ++counts[i];
    }
    if (progress && (g + 1) % 1000 == 0) std::cerr << "progress: " << g + 1 << "/" << games << "\n";
  }

  std::ofstream out(out_path);
  for (int u = 0; u <= WinTable::kMaxUnseen; ++u) {
    for (int c = 0; c < kSpreads; ++c) {
      const std::size_t i = static_cast<std::size_t>(u) * kSpreads + c;
      if (counts[i] == 0) continue;
      out << u << " " << c * WinTable::kSpreadStep - WinTable::kMaxSpread << " " << points[i] << " "
          << counts[i] << "\n";
    }
  }
  return 0;
}
