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
//       --games N --seed S [--min-bag 7] [--explore 0] [--wmp] --out FILE
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

#include "scrabble/anagram_index.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/win_table.hpp"

using namespace scrabble;

namespace {

constexpr int kBuckets = 4;
// Bag ranges: 7-20, 21-40, 41-60, 61 and more.
int Bucket(int bag) { return bag <= 20 ? 0 : bag <= 40 ? 1 : bag <= 60 ? 2 : 3; }

struct Agg {
  std::uint32_t n = 0;
  std::array<std::uint32_t, kBuckets> nb{};
  double a = 0, b = 0, c = 0;
};

struct Totals {
  std::array<std::uint64_t, kBuckets> n{};
  std::array<double, kBuckets> a{}, b{}, c{}, aa{}, bb{}, cc{};  // sums and sums of squares
};

std::uint64_t RackKey(const Rack& rack) {
  std::uint64_t key = 0;
  for (int t = 0; t < kNumTileTypes; ++t)
    for (int k = 0; k < rack.Count(Tile(t)); ++k) key = key << 5 | static_cast<std::uint64_t>(t + 1);
  return key;
}

std::uint64_t Mix(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

void Write(const std::string& path, const std::unordered_map<std::uint64_t, Agg>& racks,
           const Totals& totals) {
  std::ofstream out(path, std::ios::binary);
  out.write("KBLG1", 5);
  const std::uint32_t buckets = kBuckets;
  out.write(reinterpret_cast<const char*>(&buckets), 4);
  out.write(reinterpret_cast<const char*>(&totals), sizeof totals);
  const std::uint64_t count = racks.size();
  out.write(reinterpret_cast<const char*>(&count), 8);
  for (const auto& [key, agg] : racks) {
    out.write(reinterpret_cast<const char*>(&key), 8);
    out.write(reinterpret_cast<const char*>(&agg.n), 4);
    out.write(reinterpret_cast<const char*>(agg.nb.data()), 4 * kBuckets);
    out.write(reinterpret_cast<const char*>(&agg.a), 8);
    out.write(reinterpret_cast<const char*>(&agg.b), 8);
    out.write(reinterpret_cast<const char*>(&agg.c), 8);
  }
  if (!out) {
    std::cerr << "cannot write " << path << "\n";
    std::exit(1);
  }
}

void Read(const std::string& path, std::unordered_map<std::uint64_t, Agg>& racks, Totals& totals) {
  std::ifstream in(path, std::ios::binary);
  char magic[5];
  std::uint32_t buckets = 0;
  if (!in.read(magic, 5) || std::memcmp(magic, "KBLG1", 5) != 0 ||
      !in.read(reinterpret_cast<char*>(&buckets), 4) || buckets != kBuckets) {
    std::cerr << "not a leavegen file: " << path << "\n";
    std::exit(1);
  }
  Totals t;
  in.read(reinterpret_cast<char*>(&t), sizeof t);
  for (int i = 0; i < kBuckets; ++i) {
    totals.n[i] += t.n[i];
    totals.a[i] += t.a[i];
    totals.b[i] += t.b[i];
    totals.c[i] += t.c[i];
    totals.aa[i] += t.aa[i];
    totals.bb[i] += t.bb[i];
    totals.cc[i] += t.cc[i];
  }
  std::uint64_t count = 0;
  in.read(reinterpret_cast<char*>(&count), 8);
  for (std::uint64_t r = 0; r < count; ++r) {
    std::uint64_t key = 0;
    Agg g;
    in.read(reinterpret_cast<char*>(&key), 8);
    in.read(reinterpret_cast<char*>(&g.n), 4);
    in.read(reinterpret_cast<char*>(g.nb.data()), 4 * kBuckets);
    in.read(reinterpret_cast<char*>(&g.a), 8);
    in.read(reinterpret_cast<char*>(&g.b), 8);
    in.read(reinterpret_cast<char*>(&g.c), 8);
    Agg& dst = racks[key];
    dst.n += g.n;
    for (int i = 0; i < kBuckets; ++i) dst.nb[i] += g.nb[i];
    dst.a += g.a;
    dst.b += g.b;
    dst.c += g.c;
  }
  if (!in) {
    std::cerr << "truncated leavegen file: " << path << "\n";
    std::exit(1);
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, win_path, out_path;
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
    if (a == "--lexicon") lexicon = value();
    else if (a == "--leaves") leaves_path = value();
    else if (a == "--win-table") win_path = value();
    else if (a == "--out") out_path = value();
    else if (a == "--games") games = std::stol(value());
    else if (a == "--seed") seed = std::stoull(value());
    else if (a == "--min-bag") min_bag = std::stoi(value());
    else if (a == "--explore") explore = std::stod(value());
    else if (a == "--wmp") wmp = true;
    else if (a == "--merge") {
      while (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) merge.push_back(argv[++i]);
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
  WinTable win = WinTable::FromFile(win_path);
  AnagramIndex anagrams;
  EquityBot bot(gaddag, leaves);
  if (wmp) {
    anagrams = AnagramIndex::FromGaddag(gaddag);
    bot.set_anagram_index(&anagrams);
  }
  std::mt19937_64 rng(Mix(seed ^ 0x1EAFULL));
  std::uniform_real_distribution<double> unit(0, 1);
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
        const double w1 = after.over() ? (spread > 0 ? 1.0 : spread == 0 ? 0.5 : 0.0)
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
  std::cout << "games " << games << " samples " << samples << " racks " << racks.size() << " in "
            << secs << " s (" << games / secs << " games/s)\n";
  return 0;
}
