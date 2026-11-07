// Exact judge for the position duel: rebuilds the positions of duel.txt files
// (macondo_match --duel) from their games' GCG and, for those with a small bag,
// solves the pre-endgame for the two moves only. A move's exact value is its share
// of the bag draws it wins against the opponent's best replies (ties half).
//
//   kingbble_duel_judge --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2
//       --input DIR [--bags 1,2,3] [--time S] [--threads N] [--shard I --shards K]
//       --out FILE
// Every duel.txt under DIR is read, with the GCG files of its folder's gcg/. Each
// judged position adds the duel line, then: judged (1 or 0), our wins, Macondo's
// wins (both as shares of the draws), complete (1 if the solve finished), and the
// tiebreak spreads.

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "scrabble/equity.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/movegen.hpp"
#include "scrabble/preendgame.hpp"

using namespace scrabble;

namespace {

// A move as the duel file writes it, without the score: "8D_CAT".
std::string Label(const Move& m) {
  std::string text = m.ToString();
  if (const std::size_t p = text.rfind(" ("); p != std::string::npos) text.resize(p);
  std::replace(text.begin(), text.end(), ' ', '_');
  return text;
}

std::string WithoutScore(const std::string& label) {
  const std::size_t p = label.rfind("_(");
  return p == std::string::npos ? label : label.substr(0, p);
}

void PlaceOn(Board& board, const Move& m) {
  const int dr = m.dir == Direction::kVertical ? 1 : 0;
  for (std::size_t k = 0; k < m.tiles.size(); ++k) {
    const int r = m.row + dr * static_cast<int>(k), c = m.col + (1 - dr) * static_cast<int>(k);
    const char ch = m.tiles[k];
    if (ch == '.' || !board.IsEmpty(r, c)) continue;
    board.Place(r, c, *TileFromChar(std::toupper(static_cast<unsigned char>(ch))),
                std::islower(static_cast<unsigned char>(ch)));
  }
}

// Kingbble's position before move `ours` with `bag` tiles in the bag, if found.
std::optional<Game> FindPosition(const GcgGame& gcg, int bag, const std::string& ours) {
  Board board;
  std::array<int, 2> score{0, 0};
  int scoreless = 0;
  const int kingbble = gcg.nicknames[0] == "kingbble" ? 0 : 1;
  for (const GcgEvent& e : gcg.events) {
    const bool turn = e.kind == GcgEvent::Kind::kPlace || e.kind == GcgEvent::Kind::kPass ||
                      e.kind == GcgEvent::Kind::kExchange;
    if (!turn) continue;
    Rack own;
    for (char ch : e.rack) own.Add(ch == '?' ? kBlank : *TileFromChar(ch));
    const Rack unseen = UnseenTiles(board, own);
    if (e.player == kingbble && own.Size() == kRackSize && unseen.Size() - kRackSize == bag &&
        Label(e.move) == WithoutScore(ours)) {
      std::array<Rack, 2> racks;
      racks[kingbble] = own;
      Rack in_bag;
      int i = 0;
      for (int t = 0; t < kNumTileTypes; ++t)
        for (int n = 0; n < unseen.Count(Tile(t)); ++n, ++i)
          (i < bag ? in_bag : racks[1 - kingbble]).Add(Tile(t));
      return Game::FromPosition(board, racks, {score[0], score[1]}, kingbble, in_bag, scoreless, 1);
    }
    if (e.kind == GcgEvent::Kind::kPlace) {
      PlaceOn(board, e.move);
      score[e.player] += e.score;
      scoreless = e.score ? 0 : scoreless + 1;
    } else {
      ++scoreless;
    }
  }
  return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, input, out_path, bags_text = "1,2";
  double seconds = 120;
  int threads = 1, shard = 0, shards = 1;
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
    } else if (a == "--input") {
      input = value();
    } else if (a == "--out") {
      out_path = value();
    } else if (a == "--bags") {
      bags_text = value();
    } else if (a == "--time") {
      seconds = std::stod(value());
    } else if (a == "--threads") {
      threads = std::stoi(value());
    } else if (a == "--shard") {
      shard = std::stoi(value());
    } else if (a == "--shards") {
      shards = std::stoi(value());
    } else {
      std::cerr << "usage: " << argv[0] << " --lexicon FILE --leaves KLV --input DIR --out FILE"
                << " [--bags 1,2] [--time S] [--threads N] [--shard I --shards K]\n";
      return 2;
    }
  }
  if (lexicon.empty() || leaves_path.empty() || input.empty() || out_path.empty()) {
    std::cerr << "--lexicon, --leaves, --input and --out are required\n";
    return 2;
  }
  std::set<int> bags;
  std::stringstream list(bags_text);
  for (std::string b; std::getline(list, b, ',');) bags.insert(std::stoi(b));

  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(input))
    if (entry.path().filename() == "duel.txt") files.push_back(entry.path());
  std::sort(files.begin(), files.end());

  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  PreEndgameSolver solver(gaddag, leaves);
  MoveGenerator gen(gaddag);
  std::ofstream out(out_path, std::ios::app);
  long index = 0, judged = 0, missing = 0;
  for (const auto& file : files) {
    std::ifstream in(file);
    for (std::string line; std::getline(in, line);) {
      std::istringstream fields(line);
      int game = 0, bag = 0;
      std::string unseen, spread, seat, ours, theirs;
      if (!(fields >> game >> bag >> unseen >> spread >> seat >> ours >> theirs)) continue;
      if (!bags.count(bag) || index++ % shards != shard) continue;
      const auto gcg = ReadGcgFile(
          (file.parent_path() / "gcg" / ("game" + std::to_string(game) + ".gcg")).string());
      const std::optional<Game> position = gcg ? FindPosition(*gcg, bag, ours) : std::nullopt;
      std::vector<Move> moves;
      if (position) {
        gen.Generate(position->board(), position->rack(position->to_move()), moves);
        moves.emplace_back();  // pass
      }
      const Move* a = nullptr;
      const Move* b = nullptr;
      for (const Move& m : moves) {
        if (Label(m) == WithoutScore(ours)) a = &m;
        if (Label(m) == WithoutScore(theirs)) b = &m;
      }
      out << file.parent_path().string() << " " << line;
      if (!a || !b) {
        // Exchanges are not solved by the pre-endgame solver, and a position may not be found.
        out << " 0 0 0 0 0 0\n";
        ++missing;
        continue;
      }
      PreEndgameParams params;
      params.only = {*a, *b};
      params.max_tiles_left = -1;
      params.early_cutoff = false;
      params.threads = threads;
      params.time_limit_s = seconds;
      const PreEndgameResult r = solver.Solve(*position, params);
      double wins_a = 0, wins_b = 0, spread_a = 0, spread_b = 0;
      for (const PreEndgamePlay& p : r.plays) {
        if (p.move == *a) wins_a = p.wins / r.permutations, spread_a = p.spread / r.permutations;
        if (p.move == *b) wins_b = p.wins / r.permutations, spread_b = p.spread / r.permutations;
      }
      out << " 1 " << wins_a << " " << wins_b << " " << r.complete << " " << spread_a << " "
          << spread_b << "\n";
      out.flush();
      ++judged;
    }
  }
  std::cerr << "judged " << judged << ", not found " << missing << "\n";
  return 0;
}
