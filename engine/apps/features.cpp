// Per-option features of human turns, for fitting the move model: replays each
// GCG game and, for every turn with a known rack, ranks all legal options.
//
//   kingbble_features --lexicon lexica/NWL23.kwg --leaves lexica/NWL23.klv2
//                     --games data/raw/woogles --out data/features [--top 64] [--progress]
//
// Writes <out>/options.bin, float32 little-endian rows of the features in
// MoveFeatureNames() order (the turn's top options by static equity, then the
// played option if it ranks lower), and <out>/turns.tsv with one line per
// turn: game_id (the file stem), turn, player, rack, bag, options (all legal
// options), chosen (rank of the played option, -1 if not legal for us, e.g. a
// phony), first_row and rows in options.bin. Phony turns and turns without a
// rack are skipped.

#include "scrabble/features.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "scrabble/equity.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/movegen.hpp"

using namespace scrabble;
namespace fs = std::filesystem;

int main(int argc, char** argv) {
  static_assert(std::endian::native == std::endian::little, "options.bin is little-endian");
  std::string lexicon, leaves_path, games_dir, out_dir;
  int top = 64;
  bool progress = false;
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
      games_dir = value();
    } else if (a == "--out") {
      out_dir = value();
    } else if (a == "--top") {
      top = std::max(1, std::stoi(value()));
    } else if (a == "--progress") {
      progress = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE --leaves KLV --games DIR --out DIR [--top N] [--progress]\n";
      return 2;
    }
  }
  if (lexicon.empty() || leaves_path.empty() || games_dir.empty() || out_dir.empty()) {
    std::cerr << "--lexicon, --leaves, --games and --out are required\n";
    return 2;
  }
  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(games_dir))
    if (e.path().extension() == ".gcg") files.push_back(e.path());
  std::sort(files.begin(), files.end());

  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  MoveGenerator gen(gaddag);
  fs::create_directories(out_dir);
  std::ofstream bin(fs::path(out_dir) / "options.bin", std::ios::binary);
  std::ofstream tsv(fs::path(out_dir) / "turns.tsv");
  tsv << "game_id\tturn\tplayer\track\tbag\toptions\tchosen\tfirst_row\trows\n";

  long long rows_written = 0, turns = 0, unmatched = 0;
  int failed = 0;
  auto write_row = [&](const Option& o) {
    bin.write(reinterpret_cast<const char*>(o.features.data()), sizeof(o.features));
    ++rows_written;
  };
  for (std::size_t g = 0; g < files.size(); ++g) {
    std::string error;
    auto game = ReadGcgFile(files[g].string(), &error);
    std::vector<ReplayedTurn> replayed;
    if (game) replayed = ReplayTurns(*game, &error);
    if (replayed.empty()) {
      ++failed;
      std::cerr << files[g].filename().string() << ": " << error << "\n";
    }
    for (const ReplayedTurn& t : replayed) {
      if (!t.known_rack || t.phony) continue;
      Rack unseen = UnseenTiles(t.board, t.rack);
      std::vector<Option> options = RankOptions(gen, t.board, t.rack, t.bag_size, unseen, leaves);
      const int chosen = FindOption(options, t.move);
      const long long first = rows_written;
      const int n = std::min(static_cast<int>(options.size()), top);
      for (int i = 0; i < n; ++i) write_row(options[i]);
      if (chosen >= n) write_row(options[chosen]);
      unmatched += chosen < 0;
      ++turns;
      tsv << files[g].stem().string() << '\t' << t.turn << '\t' << t.player << '\t'
          << t.rack.ToString() << '\t' << t.bag_size << '\t' << options.size() << '\t' << chosen
          << '\t' << first << '\t' << rows_written - first << '\n';
    }
    if (progress) std::cerr << "progress: " << g + 1 << "/" << files.size() << std::endl;
  }
  std::cout << "games: " << files.size() - failed << " (" << failed << " failed), turns: " << turns
            << " (" << unmatched << " with an option we do not generate), rows: " << rows_written
            << "\n";
  return 0;
}
