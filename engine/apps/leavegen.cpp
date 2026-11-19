// Leave-generation data (the full-rack method of wolges and Magpie): fast
// equity self-play that, for every turn begun with a full rack and at least
// --min-bag tiles in the bag, credits that rack with three targets, summed per
// rack and per bag range:
//   a  the static equity of the move made (its score plus the value of the
//      leave it keeps, by the current leaves), the wolges/Magpie target;
//   b  the change in the mover's chance to win the move makes, by the win
//      table, counting the kept leave at its current value;
//   c  the spread the mover gains from this turn to the end of the game.
// A leave's value is later derived from the racks it can be completed to,
// weighted by their draw chances (scripts/fit_leaves.py, run off GitHub).
// Racks are dealt, not chosen, so their sample carries no selection bias from
// the policy; --explore P still lets the mover pick uniformly among its 10 best
// moves with probability P.
//
//   kingbble_leavegen --lexicon lexica/CSW24.txt --leaves LEAVES --win-table CSV
//       --games N --seed S [--min-bag 7] [--explore 0] [--wmp] [--leave-deltas CSV] --out FILE
//   kingbble_leavegen --merge FILE... --out FILE
//
// FILE is binary: a header ("KBLG1", bucket edges, per-bucket counts and sums)
// then one record per rack: key (u64, 5 bits a tile, sorted), count (u32),
// count per bag range (4 x u32), and the sums of a, b and c (3 x f64).

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "leavegen_io.hpp"
#include "scrabble/anagram_index.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/inference.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/win_table.hpp"

using namespace scrabble;
using namespace leavegen;

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, win_path, out_path, deltas_path;
  // Rare racks (wolges' min_samples_per_rack, Magpie's rare partition): with
  // probability --force-share, a counted turn is dealt instead one of the full
  // racks this process has seen fewer than --min-samples times, when the
  // mover's rack and the bag hold its tiles. Its sample feeds only that rack's
  // sums, as any turn's does.
  double force_share = 0;
  std::uint32_t min_samples = 0;
  std::vector<std::string> merge;
  long games = 1000;
  std::uint64_t seed = 1;
  int min_bag = 7;
  double explore = 0;
  bool wmp = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << a << "\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--lexicon")
      lexicon = value();
    else if (a == "--leaves")
      leaves_path = value();
    else if (a == "--win-table")
      win_path = value();
    else if (a == "--out")
      out_path = value();
    else if (a == "--games")
      games = std::stol(value());
    else if (a == "--seed")
      seed = std::stoull(value());
    else if (a == "--min-bag")
      min_bag = std::stoi(value());
    else if (a == "--explore")
      explore = std::stod(value());
    else if (a == "--wmp")
      wmp = true;
    else if (a == "--leave-deltas")
      deltas_path = value();
    else if (a == "--force-share")
      force_share = std::stod(value());
    else if (a == "--min-samples")
      min_samples = static_cast<std::uint32_t>(std::stoul(value()));
    else if (a == "--merge") {
      while (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0)
        merge.push_back(argv[++i]);
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE --leaves FILE --win-table CSV --games N --seed S [--min-bag 7]"
                   " [--explore P] [--wmp] --out FILE\n       "
                << argv[0] << " --merge FILE... --out FILE\n";
      return 2;
    }
  }
  if (out_path.empty()) {
    std::cerr << "--out is required\n";
    return 2;
  }
  std::unordered_map<std::uint64_t, Agg> racks;
  Totals totals;
  if (!merge.empty()) {
    for (const std::string& f : merge) Read(f, racks, totals);
    Write(out_path, racks, totals);
    std::cout << "merged " << merge.size() << " files: " << racks.size() << " racks\n";
    return 0;
  }
  if (lexicon.empty() || leaves_path.empty() || win_path.empty()) {
    std::cerr << "--lexicon, --leaves and --win-table are required\n";
    return 2;
  }
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  if (!deltas_path.empty()) leaves.AddDeltas(deltas_path);  // in memory only
  WinTable win = WinTable::FromFile(win_path);
  AnagramIndex anagrams;
  EquityBot bot(gaddag, leaves);
  if (wmp) {
    anagrams = AnagramIndex::FromGaddag(gaddag);
    bot.set_anagram_index(&anagrams);
  }
  std::mt19937_64 rng(Mix(seed ^ 0x1EAFULL));
  std::uniform_real_distribution<double> unit(0, 1);
  // Every full rack the bag allows, as RackKey keys, for the rare-rack draws.
  std::vector<std::uint64_t> rare;
  if (force_share > 0 && min_samples > 0) {
    const auto& tiles = EnglishTiles();
    std::array<int, kNumTileTypes> c{};
    auto all = [&](auto& self, int t, int left) -> void {
      if (left == 0) {
        Rack r;
        for (int u = 0; u < kNumTileTypes; ++u)
          for (int k = 0; k < c[u]; ++k) r.Add(Tile(u));
        rare.push_back(RackKey(r));
        return;
      }
      if (t == kNumTileTypes) return;
      for (int k = std::min<int>(left, tiles[t].count); k >= 0; --k) {
        c[t] = k;
        self(self, t + 1, left - k);
      }
      c[t] = 0;
    };
    all(all, 0, kRackSize);
    std::cout << "rare-rack pool: " << rare.size() << " full racks" << std::endl;
  }
  long forced = 0;
  struct Turn {
    std::uint64_t key;
    int bucket, player, before;
    double a, b;
  };
  std::vector<Turn> turns;
  const auto start = std::chrono::steady_clock::now();
  long samples = 0;
  for (long g = 0; g < games; ++g) {
    Game game(Mix(Mix(seed) + static_cast<std::uint64_t>(g)));
    turns.clear();
    while (!game.over()) {
      const int p = game.to_move();
      if (!rare.empty() && game.bag_size() >= min_bag && game.rack(p).Size() == kRackSize &&
          unit(rng) < force_share) {
        // Deal an under-sampled rack in place of this one, if its tiles are at hand.
        const Rack pool = MinusTiles(UnseenTiles(game.board(), Rack()), game.rack(1 - p));
        for (int attempt = 0; attempt < 16 && !rare.empty(); ++attempt) {
          const std::size_t i = std::uniform_int_distribution<std::size_t>(0, rare.size() - 1)(rng);
          const std::uint64_t key = rare[i];
          const auto it = racks.find(key);
          if (it != racks.end() && it->second.n >= min_samples) {
            rare[i] = rare.back();
            rare.pop_back();
            continue;
          }
          Rack want;
          for (std::uint64_t k = key; k; k >>= 5) want.Add(Tile((k & 31) - 1));
          if (!ContainsTiles(pool, want)) continue;
          std::array<Rack, 2> deal{game.rack(0), game.rack(1)};
          deal[p] = want;
          game = Game::FromPosition(game.board(), deal, {game.score(0), game.score(1)}, p,
                                    MinusTiles(pool, want), game.scoreless_turns(), rng());
          ++forced;
          break;
        }
      }
      const Rack rack = game.rack(p);
      const int bag = game.bag_size();
      const bool counted = bag >= min_bag && rack.Size() == kRackSize;
      Move m;
      if (explore > 0 && unit(rng) < explore) {
        std::vector<RankedMove> ranked = bot.Rank(game);
        const std::size_t top = std::min<std::size_t>(ranked.size(), 10);
        m = ranked[std::uniform_int_distribution<std::size_t>(0, top - 1)(rng)].move;
      } else {
        m = bot.Choose(game);
      }
      if (counted) {
        const Rack unseen = UnseenTiles(game.board(), rack);
        const int before = game.score(p) - game.score(1 - p);
        const double eq = StaticEquity(m, rack, bag, unseen, leaves, 0);
        const double kept = LeaveValue(LeaveAfter(rack, m), bag, leaves, 0);
        const double w0 = win.Win(before, unseen.Size());
        Game after = game;
        after.Apply(m);
        const int spread = after.score(p) - after.score(1 - p);
        // The opponent moves next: their chance, from their own spread and view.
        const int opp_unseen = UnseenTiles(after.board(), after.rack(1 - p)).Size();
        const double w1 = after.over() ? (spread > 0    ? 1.0
                                          : spread == 0 ? 0.5
                                                        : 0.0)
                                       : 1 - win.Win(-(spread + kept), opp_unseen);
        turns.push_back({RackKey(rack), Bucket(bag), p, before, eq, w1 - w0});
        game = std::move(after);
      } else {
        game.Apply(m);
      }
    }
    for (const Turn& t : turns) {
      const double c = (game.score(t.player) - game.score(1 - t.player)) - t.before;
      Agg& agg = racks[t.key];
      ++agg.n;
      ++agg.nb[t.bucket];
      agg.a += t.a;
      agg.b += t.b;
      agg.c += c;
      totals.n[t.bucket] += 1;
      totals.a[t.bucket] += t.a;
      totals.b[t.bucket] += t.b;
      totals.c[t.bucket] += c;
      totals.aa[t.bucket] += t.a * t.a;
      totals.bb[t.bucket] += t.b * t.b;
      totals.cc[t.bucket] += c * c;
      ++samples;
    }
  }
  Write(out_path, racks, totals);
  const double secs =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  if (forced)
    std::cout << "forced turns " << forced << ", racks still rare " << rare.size() << "\n";
  std::cout << "games " << games << " samples " << samples << " racks " << racks.size() << " in "
            << secs << " s (" << games / secs << " games/s)\n";
  return 0;
}
