// Exact judge of the endgames in GCG games: every move made with the bag empty
// (both racks known) is scored against best play. For each such move it solves
// the position before it and the one after, and writes one line:
//   file turn player best played loss exact
// with `best` the spread the mover gains from there with best play by both
// sides, `played` what the move made gains (its points, plus the end-of-game
// rack swing if it ends the game, minus the opponent's best from the position
// after it) and `loss` their difference (0 for a best move). `exact` is 1 when
// both solves finished.
//
//   kingbble_endgame_judge --lexicon lexica/CSW24.txt --input DIR [--plies 25]
//       [--time S] [--threads N] [--shard I --shards K] --out FILE
// Every *.gcg under DIR is read; with --shard I --shards K only the files whose
// position in the sorted list is I modulo K.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "scrabble/endgame.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/gcg.hpp"

using namespace scrabble;

int main(int argc, char** argv) {
  std::string lexicon, input, out_path;
  int plies = 25, threads = 1, shard = 0, shards = 1;
  double time_s = 10;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << a << "\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--lexicon")
      lexicon = value();
    else if (a == "--input")
      input = value();
    else if (a == "--out")
      out_path = value();
    else if (a == "--plies")
      plies = std::stoi(value());
    else if (a == "--time")
      time_s = std::stod(value());
    else if (a == "--threads")
      threads = std::stoi(value());
    else if (a == "--shard")
      shard = std::stoi(value());
    else if (a == "--shards")
      shards = std::stoi(value());
    else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE --input DIR [--plies N] [--time S] [--threads N]"
                   " [--shard I --shards K] --out FILE\n";
      return 2;
    }
  }
  if (lexicon.empty() || input.empty() || out_path.empty()) {
    std::cerr << "--lexicon, --input and --out are required\n";
    return 2;
  }
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  EndgameSolver solver(gaddag);
  solver.set_threads(threads);
  std::vector<std::string> files;
  for (const auto& e : std::filesystem::recursive_directory_iterator(input))
    if (e.is_regular_file() && e.path().extension() == ".gcg") files.push_back(e.path().string());
  std::sort(files.begin(), files.end());
  std::ofstream out(out_path, std::ios::app);
  long judged = 0;
  for (std::size_t f = 0; f < files.size(); ++f) {
    if (static_cast<int>(f % shards) != shard) continue;
    std::optional<GcgGame> gcg = ReadGcgFile(files[f]);
    if (!gcg) continue;
    Board board;
    std::array<int, 2> score{0, 0};
    int scoreless = 0, turn = 0;
    for (const GcgEvent& e : gcg->events) {
      const bool is_turn = e.kind == GcgEvent::Kind::kPlace || e.kind == GcgEvent::Kind::kPass ||
                           e.kind == GcgEvent::Kind::kExchange;
      if (!is_turn) continue;
      ++turn;
      Rack own;
      for (char ch : e.rack) own.Add(ch == '?' ? kBlank : *TileFromChar(ch));
      const Rack unseen = UnseenTiles(board, own);
      if (!e.rack.empty() && unseen.Size() <= kRackSize && e.kind != GcgEvent::Kind::kExchange) {
        // The bag is empty: what the mover cannot see is the opponent's rack.
        std::array<Rack, 2> racks;
        racks[e.player] = own;
        racks[1 - e.player] = unseen;
        const Game before =
            Game::FromPosition(board, racks, {score[0], score[1]}, e.player, Rack(), scoreless, 1);
        const EndgameResult best = solver.Solve(before, plies, time_s);
        Game after = before;
        Move m = e.kind == GcgEvent::Kind::kPlace ? e.move : Move{};
        try {
          after.Apply(m);
        } catch (const std::exception&) {
          break;
        }
        const int me = e.player;
        const int gained =
            (after.score(me) - after.score(1 - me)) - (before.score(me) - before.score(1 - me));
        bool exact = best.exact;
        int played = gained;
        if (!after.over()) {
          const EndgameResult reply = solver.Solve(after, plies, time_s);
          played -= reply.value;
          exact = exact && reply.exact;
        }
        out << files[f] << " " << turn << " " << gcg->nicknames[me] << " " << best.value << " "
            << played << " " << best.value - played << " " << exact << "\n";
        ++judged;
      }
      if (e.kind == GcgEvent::Kind::kPlace) {
        // PlaceOn, as the duel judge does: the move's new tiles go on the board.
        const int dr = e.move.dir == Direction::kVertical ? 1 : 0;
        for (std::size_t k = 0; k < e.move.tiles.size(); ++k) {
          const int r = e.move.row + dr * static_cast<int>(k);
          const int c = e.move.col + (1 - dr) * static_cast<int>(k);
          const char ch = e.move.tiles[k];
          if (ch == '.' || !board.IsEmpty(r, c)) continue;
          board.Place(r, c, *TileFromChar(std::toupper(static_cast<unsigned char>(ch))),
                      std::islower(static_cast<unsigned char>(ch)));
        }
        score[e.player] += e.score;
        scoreless = e.score ? 0 : scoreless + 1;
      } else {
        ++scoreless;
      }
    }
    out.flush();
  }
  std::cout << "judged " << judged << " endgame moves\n";
  return 0;
}
