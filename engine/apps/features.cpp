// Per-option features of human turns, for fitting the move model: replays each
// GCG game and, for every turn with a known rack, ranks all legal options.
//
//   kingbble_features --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2
//                     --games data/raw/woogles --out data/features [--top 128]
//                     [--turn-info FILE] [--word-freq TSV] [--alt-lexicon FILE] [--progress]
//
// Writes <out>/options.bin: a header (the bytes "KBFT", then uint32 version,
// feature count and names length, the space-separated names from
// MoveFeatureNames() padded with spaces to a multiple of 4 bytes), then float32
// little-endian rows of the features (the turn's top options by static equity,
// then the played option if it ranks lower); and <out>/turns.tsv with one line
// per turn: game_id (the file stem), turn, player, rack, bag, options (all
// legal options), chosen (rank of the played option, -1 if not legal for us,
// e.g. a phony), first_row and rows in options.bin. Phony turns and turns
// without a rack are skipped.
//
// --turn-info lists "game_id<TAB>turn<TAB>rating<TAB>split<TAB>is_bot" lines
// (the turn store's view); the rating feeds the context features.
//
//   kingbble_features --lexicon ... --games DIR --turn-info FILE --count-words OUT
//
// only counts the main words of human placements in the train split, writing
// "WORD<TAB>count" lines to OUT (the play counts of ml/data/word_freq.py).

#include "scrabble/features.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "scrabble/equity.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/movegen.hpp"

using namespace scrabble;
namespace fs = std::filesystem;

namespace {

constexpr std::uint32_t kFormatVersion = 2;

struct TurnInfo {
  int rating = 0;
  std::string split;
  bool is_bot = false;
};

std::map<std::pair<std::string, int>, TurnInfo> ReadTurnInfo(const std::string& path) {
  std::map<std::pair<std::string, int>, TurnInfo> out;
  if (path.empty()) return out;
  std::ifstream in(path);
  if (!in) {
    std::cerr << "cannot open " << path << "\n";
    std::exit(1);
  }
  for (std::string line; std::getline(in, line);) {
    std::istringstream ss(line);
    std::string id;
    int turn = 0, bot = 0;
    TurnInfo info;
    if (ss >> id >> turn >> info.rating >> info.split >> bot) {
      info.is_bot = bot != 0;
      out[{id, turn}] = info;
    }
  }
  return out;
}

void WriteHeader(std::ostream& bin) {
  std::string names;
  for (const char* n : MoveFeatureNames()) names += std::string(names.empty() ? "" : " ") + n;
  names.resize((names.size() + 3) / 4 * 4, ' ');
  const std::uint32_t fields[3] = {kFormatVersion, static_cast<std::uint32_t>(kNumMoveFeatures),
                                   static_cast<std::uint32_t>(names.size())};
  bin.write("KBFT", 4);
  bin.write(reinterpret_cast<const char*>(fields), sizeof(fields));
  bin.write(names.data(), static_cast<std::streamsize>(names.size()));
}

}  // namespace

int main(int argc, char** argv) {
  static_assert(std::endian::native == std::endian::little, "options.bin is little-endian");
  std::string lexicon, leaves_path, games_dir, out_dir, info_path, freq_path, alt_path, count_path;
  int top = 128;
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
    } else if (a == "--turn-info") {
      info_path = value();
    } else if (a == "--word-freq") {
      freq_path = value();
    } else if (a == "--alt-lexicon") {
      alt_path = value();
    } else if (a == "--count-words") {
      count_path = value();
    } else if (a == "--progress") {
      progress = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE --games DIR (--leaves KLV --out DIR [--top N]"
                   " [--word-freq TSV] [--alt-lexicon FILE] | --count-words OUT)"
                   " [--turn-info FILE] [--progress]\n";
      return 2;
    }
  }
  const bool counting = !count_path.empty();
  if (lexicon.empty() || games_dir.empty() ||
      (!counting && (leaves_path.empty() || out_dir.empty()))) {
    std::cerr << "--lexicon and --games, and --leaves and --out or --count-words, are required\n";
    return 2;
  }
  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(games_dir))
    if (e.path().extension() == ".gcg") files.push_back(e.path());
  std::sort(files.begin(), files.end());
  const auto info = ReadTurnInfo(info_path);
  auto info_of = [&](const std::string& id, int turn) -> const TurnInfo* {
    auto it = info.find({id, turn});
    return it == info.end() ? nullptr : &it->second;
  };

  if (counting) {
    std::map<std::string, long long> counts;
    for (const fs::path& f : files) {
      auto game = ReadGcgFile(f.string());
      if (!game) continue;
      for (const ReplayedTurn& t : ReplayTurns(*game)) {
        const TurnInfo* ti = info_of(f.stem().string(), t.turn);
        if (!ti || ti->split != "train" || ti->is_bot || t.phony) continue;
        if (t.move.kind == Move::Kind::kPlace) ++counts[MainWord(t.board, t.move)];
      }
    }
    std::ofstream out(count_path);
    for (const auto& [word, n] : counts) out << word << '\t' << n << '\n';
    std::cout << "words: " << counts.size() << "\n";
    return 0;
  }

  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  FeatureLexicon words = FeatureLexicon::Load(lexicon, freq_path, alt_path);
  FeatureComputer computer(leaves, &words);
  MoveGenerator gen(gaddag);
  fs::create_directories(out_dir);
  std::ofstream bin(fs::path(out_dir) / "options.bin", std::ios::binary);
  std::ofstream tsv(fs::path(out_dir) / "turns.tsv");
  WriteHeader(bin);
  tsv << "game_id\tturn\tplayer\track\tbag\toptions\tchosen\tfirst_row\trows\n";

  long long rows_written = 0, turns = 0, unmatched = 0, filled = 0;
  double rank_seconds = 0, fill_seconds = 0;
  using Clock = std::chrono::steady_clock;
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
    const std::string id = files[g].stem().string();
    for (const ReplayedTurn& t : replayed) {
      if (!t.known_rack || t.phony) continue;
      const TurnInfo* ti = info_of(id, t.turn);
      Rack unseen = UnseenTiles(t.board, t.rack);
      const auto t0 = Clock::now();
      std::vector<Option> options = RankOptions(gen, t.board, t.rack, t.bag_size, unseen, leaves);
      const int chosen = FindOption(options, t.move);
      const int n = std::min(static_cast<int>(options.size()), top);
      const auto t1 = Clock::now();
      const TurnContext ctx{t.bag_size, t.score_diff, t.turn, t.prev_exchange, ti ? ti->rating : 0};
      // The --word-freq play counts include this turn when it is one they count
      // (as --count-words does): leave it out of its own word's count.
      const bool counted =
          ti && ti->split == "train" && !ti->is_bot && t.move.kind == Move::Kind::kPlace;
      computer.Fill(t.board, t.rack, ctx, options, n, chosen,
                    counted ? MainWord(t.board, t.move) : std::string());
      const auto t2 = Clock::now();
      rank_seconds += std::chrono::duration<double>(t1 - t0).count();
      fill_seconds += std::chrono::duration<double>(t2 - t1).count();
      filled += n + (chosen >= n);
      const long long first = rows_written;
      for (int i = 0; i < n; ++i) write_row(options[i]);
      if (chosen >= n) write_row(options[chosen]);
      unmatched += chosen < 0;
      ++turns;
      tsv << id << '\t' << t.turn << '\t' << t.player << '\t' << t.rack.ToString() << '\t'
          << t.bag_size << '\t' << options.size() << '\t' << chosen << '\t' << first << '\t'
          << rows_written - first << '\n';
    }
    if (progress) std::cerr << "progress: " << g + 1 << "/" << files.size() << std::endl;
  }
  std::cout << "games: " << files.size() - failed << " (" << failed << " failed), turns: " << turns
            << " (" << unmatched << " with an option we do not generate), rows: " << rows_written
            << "\n";
  if (turns > 0 && filled > 0)
    std::cout << "time: ranking " << 1e3 * rank_seconds / turns << " ms per turn, features "
              << 1e6 * fill_seconds / filled << " us per candidate\n";
  return 0;
}
