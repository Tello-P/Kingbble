// Dumps positions from greedy self-play as CGP lines, optionally followed by
// every legal placement our generator finds (for differential tests).
//
//   kingbble_positions --lexicon lexica/ENABLE.txt --count 10000 --seed 1 [--moves]
//
// With --moves each position line is followed by "<n>" and n lines
// "<coord> <word> (<score>)", where the word uses '.' for played-through squares.
//
//   kingbble_positions --lexicon lexica/ENABLE.txt --count 100 --endgame [--solve]
//
// With --endgame only the first empty-bag position of each game is written (or,
// with --max-tiles N, the first one with at most N tiles on both racks); --solve
// follows it with "<value>", our endgame solver's value for the player to move.
// --shard J/N keeps only the positions whose index is J modulo N, so that N
// processes split the same positions; --only I,J,... (also with --preendgame)
// keeps only the positions with those indices (from 0).
//
//   kingbble_positions --lexicon lexica/ENABLE.txt --leaves lexica/NWL23.klv2 --count 100
//                      --preendgame 2 [--solve]
//
// With --preendgame N only the first position of each game with 1..N tiles in
// the bag is written; --solve follows it with a JSON line holding our
// pre-endgame solution: {"permutations", "endgames", "seconds", "plays": [{"move",
// "wins", "spread"}]} with the analysed plays, best first ("spread" is null
// without a tiebreak). --candidates N analyses only the N best plays by static
// equity (and passing, with --pass); --exhaustive analyses every candidate fully
// (no early cutoff, whatever it leaves in the bag, passing considered in nested
// pre-endgames too); --endgame-plies sets the endgame depth; --threads the
// number of solver threads.
//
// --draws "MOVE" (with --preendgame, e.g. --draws "M5 BLADE") follows each
// position instead with one line per draw of the bag for that play, when it
// empties the bag: {"draw", "ways", "value", "final", "cgp"}: the tiles drawn,
// the ordered draws it stands for, the endgame value for the opponent to move,
// our final spread and the endgame position (for another solver); then {"move", "permutations",
// "spread"} with the sum of ways x final, the pre-endgame tiebreak spread (to compare it draw by
// draw).
//
// --time-limit S caps each solve at S seconds; a position not solved exactly
// within it prints "skip" (endgame) or {"skip": true} (pre-endgame).

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

#include "scrabble/cgp.hpp"
#include "scrabble/endgame.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/greedy_bot.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/movegen.hpp"
#include "scrabble/preendgame.hpp"

using namespace scrabble;

namespace {

std::string PreEndgameJson(const PreEndgameResult& r, double seconds) {
  std::string out = "{\"permutations\": " + std::to_string(static_cast<long long>(r.permutations)) +
                    ", \"endgames\": " + std::to_string(r.endgames) +
                    ", \"seconds\": " + std::to_string(seconds) + ", \"plays\": [";
  bool first = true;
  for (const PreEndgamePlay& p : r.plays) {
    if (p.skipped || p.cut) continue;
    if (!first) out += ", ";
    first = false;
    out += "{\"move\": \"" + (p.move.kind == Move::Kind::kPass ? "pass" : p.move.ToString()) +
           "\", \"wins\": " + std::to_string(p.wins) + ", \"spread\": " +
           (p.has_spread ? std::to_string(static_cast<long long>(p.spread)) : "null") + "}";
  }
  return out + "]}";
}

// Per-draw final spreads of `play` (which empties the bag) in `game`, as
// PreEndgameSolver's tiebreak sums them: the bag tiles from the unseen ones in
// every combination, the opponent holding the rest, then the endgame solved.
void PrintDraws(const Game& game, const Gaddag& gaddag, const std::string& play, double time_limit,
                const std::string& lexicon_name) {
  const int me = game.to_move();
  MoveGenerator gen(gaddag);
  const Move* found = nullptr;
  const auto moves = gen.Generate(game.board(), game.rack(me));
  for (const Move& m : moves)
    if (m.ToString().rfind(play + " ", 0) == 0) found = &m;
  const int k = game.bag_size();
  if (!found || found->TilesPlayed() < k) {
    std::cout << "{\"move\": \"" << play << "\", \"error\": \"not a play that empties the bag\"}"
              << std::endl;
    return;
  }
  const Rack unseen = UnseenTiles(game.board(), game.rack(me));
  EndgameSolver solver(gaddag);
  double permutations = 0, spread = 0;
  // Multisets of k tiles from the unseen ones, each with its ordered draws.
  std::function<void(int, Rack, Rack, double, int)> rec = [&](int from, Rack pool, Rack bag,
                                                              double ways, int left) {
    if (left == 0) {
      std::array<Rack, 2> racks;
      racks[me] = game.rack(me);
      racks[1 - me] = pool;
      const std::array<int, 2> scores{game.score(0), game.score(1)};
      Game g = Game::FromPosition(game.board(), racks, scores, me, bag, game.scoreless_turns(), 1);
      g.Apply(*found);
      int value = 0, final = g.score(me) - g.score(1 - me);
      if (!g.over()) {
        const EndgameResult r = solver.Solve(g, 25, time_limit);
        value = r.value;
        final -= value;
      }
      permutations += ways;
      spread += ways * final;
      std::cout << "{\"draw\": \"" << bag.ToString() << "\", \"ways\": " << ways
                << ", \"value\": " << value << ", \"final\": " << final << ", \"cgp\": \""
                << GameToCgp(g, lexicon_name) << "\"}" << std::endl;
      return;
    }
    for (int t = from; t < kNumTileTypes; ++t) {
      int n = pool.Count(Tile(t));
      if (n == 0) continue;
      // Taking c tiles of this letter: n * (n-1) * ... ordered choices, placed
      // among the draws already made.
      Rack p = pool, b = bag;
      double w = ways;
      for (int c = 1; c <= std::min(n, left); ++c) {
        p.Remove(Tile(t));
        b.Add(Tile(t));
        w *= double(n - c + 1) * (k - left + c) / c;
        rec(t + 1, p, b, w, left - c);
      }
    }
  };
  rec(0, unseen, Rack(), 1, k);
  std::cout << "{\"move\": \"" << found->ToString() << "\", \"permutations\": " << permutations
            << ", \"spread\": " << static_cast<long long>(spread) << "}" << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_file, lexicon_name = "ENABLE";
  int count = 100;
  int max_tiles = 2 * kRackSize;
  int preendgame = 0;
  PreEndgameParams peg_params;
  std::uint64_t seed = 1;
  int shard = 0, shards = 1;
  std::set<int> only;  // with --only, the indices of the positions to write
  double time_limit = 0;
  bool with_moves = false, endgame = false, solve = false;
  std::string draws;  // with --draws, the play whose per-draw spreads are printed
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
      leaves_file = value();
    } else if (a == "--preendgame") {
      preendgame = std::stoi(value());
    } else if (a == "--candidates") {
      peg_params.candidates = std::stoi(value());
    } else if (a == "--pass") {
      peg_params.pass_candidate = true;
    } else if (a == "--exhaustive") {
      peg_params.early_cutoff = false;
      peg_params.max_tiles_left = -1;
      peg_params.skip_deep_pass = false;
    } else if (a == "--only") {
      std::stringstream list(value());
      for (std::string item; std::getline(list, item, ',');)
        if (!item.empty()) only.insert(std::stoi(item));
    } else if (a == "--shard") {
      const std::string v = value();
      const auto slash = v.find('/');
      if (slash == std::string::npos) {
        std::cerr << "--shard takes J/N\n";
        return 2;
      }
      shard = std::stoi(v.substr(0, slash));
      shards = std::stoi(v.substr(slash + 1));
    } else if (a == "--draws") {
      draws = value();
    } else if (a == "--time-limit") {
      time_limit = std::stod(value());
    } else if (a == "--threads") {
      peg_params.threads = std::stoi(value());
    } else if (a == "--endgame-plies") {
      peg_params.endgame_plies = std::stoi(value());
    } else if (a == "--name") {
      lexicon_name = value();
    } else if (a == "--count") {
      count = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--moves") {
      with_moves = true;
    } else if (a == "--endgame") {
      endgame = true;
    } else if (a == "--max-tiles") {
      max_tiles = std::stoi(value());
    } else if (a == "--solve") {
      solve = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE [--name LEX] [--count N] [--seed S] [--moves]"
                   " [--endgame [--max-tiles N] [--solve] [--shard J/N]] [--only I,J,...]"
                   " [--leaves FILE --preendgame N [--solve] [--candidates N] [--pass]"
                   " [--exhaustive] [--endgame-plies N] [--threads N] [--draws MOVE]]"
                   " [--time-limit S]\n";
      return 2;
    }
  }
  if (lexicon.empty() || (preendgame > 0 && leaves_file.empty())) {
    std::cerr << "--lexicon is required (and --leaves with --preendgame)\n";
    return 2;
  }

  Gaddag gaddag = Gaddag::FromFile(lexicon);
  GreedyBot bot(gaddag);
  MoveGenerator gen(gaddag);
  EndgameSolver solver(gaddag);
  Leaves leaves = leaves_file.empty() ? Leaves() : Leaves::FromFile(leaves_file);
  PreEndgameSolver peg(gaddag, leaves);
  int written = 0;
  for (std::uint64_t g = 0; written < count; ++g) {
    Game game(seed * 1000003 + g);
    if (preendgame > 0) {
      while (!game.over() && game.bag_size() > preendgame) game.Apply(bot.Choose(game));
      if (game.over() || game.bag_size() == 0) continue;
      if (!only.empty() && !only.count(written)) {
        ++written;
        continue;
      }
      std::cout << GameToCgp(game, lexicon_name) << "\n";
      if (!draws.empty()) {
        PrintDraws(game, gaddag, draws, time_limit, lexicon_name);
      } else if (solve) {
        const auto start = std::chrono::steady_clock::now();
        PreEndgameParams params = peg_params;
        params.time_limit_s = time_limit;
        PreEndgameResult r = peg.Solve(game, params);
        const double secs =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (r.complete)
          std::cout << PreEndgameJson(r, secs) << std::endl;
        else
          std::cout << "{\"skip\": true}" << std::endl;
      }
      ++written;
      continue;
    }
    if (endgame) {
      while (!game.over() &&
             (game.bag_size() > 0 || game.rack(0).Size() + game.rack(1).Size() > max_tiles))
        game.Apply(bot.Choose(game));
      if (game.over()) continue;
      const int index = written++;
      if (index % shards != shard || (!only.empty() && !only.count(index))) continue;
      std::cout << GameToCgp(game, lexicon_name) << "\n";
      if (solve) {
        const EndgameResult r = solver.Solve(game, 25, time_limit);
        if (r.exact)
          std::cout << r.value << std::endl;
        else
          std::cout << "skip" << std::endl;
      }
      continue;
    }
    while (!game.over() && written < count) {
      std::cout << GameToCgp(game, lexicon_name) << "\n";
      if (with_moves) {
        auto moves = gen.Generate(game.board(), game.rack(game.to_move()));
        std::cout << moves.size() << "\n";
        for (const Move& m : moves) std::cout << m.ToString() << "\n";
      }
      ++written;
      game.Apply(bot.Choose(game));
    }
  }
  return 0;
}
