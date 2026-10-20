// Match against Quackle: Kingbble's bot plays a Quackle computer player run by
// scripts/quackle_match/quackle_helper, on the same lexicon (the helper's
// --words output). Kingbble's Game keeps the bag; Quackle is sent the game so
// far as GCG and its rack. Seats swap every game; every game is saved as GCG.
//
//   kingbble_quackle_match --lexicon build/NWL23.txt --leaves lexica/NWL23.klv2
//       --helper build/quackle_helper/quackle_helper --bot equity+peg+eg
//       --games 100 --seed 1 [--quackle-player "Championship Player"]
//       [--quackle-lexicon nwl23] [--quackle-threads N] [--gcg-dir DIR]
// Bot options as kingbble_selfplay: --threads, --endgame-time, --peg-bag,
// --peg-candidates, --peg-time, --apeg-*, --sim-*.

#include <sys/wait.h>
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "bot_factory.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/movegen.hpp"

using namespace scrabble;

namespace {

std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

// The helper process, spoken to over its stdin and stdout.
class Helper {
 public:
  explicit Helper(const std::vector<std::string>& argv) {
    int to[2], from[2];
    if (pipe(to) != 0 || pipe(from) != 0) throw std::runtime_error("pipe failed");
    pid_ = fork();
    if (pid_ < 0) throw std::runtime_error("fork failed");
    if (pid_ == 0) {
      dup2(to[0], STDIN_FILENO);
      dup2(from[1], STDOUT_FILENO);
      close(to[1]);
      close(from[0]);
      std::vector<char*> args;
      for (const std::string& a : argv) args.push_back(const_cast<char*>(a.c_str()));
      args.push_back(nullptr);
      execv(args[0], args.data());
      _exit(127);
    }
    close(to[0]);
    close(from[1]);
    in_ = fdopen(to[1], "w");
    out_ = fdopen(from[0], "r");
  }
  ~Helper() {
    fclose(in_);
    fclose(out_);
    waitpid(pid_, nullptr, 0);
  }

  std::string Ask(const std::string& gcg, const Rack& rack) {
    const std::string request = gcg + "RACK " + rack.ToString() + "\n";
    fputs(request.c_str(), in_);
    fflush(in_);
    char buf[256];
    if (!fgets(buf, sizeof buf, out_)) throw std::runtime_error("the Quackle helper stopped");
    std::string line = buf;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    return line;
  }

 private:
  pid_t pid_;
  FILE* in_;
  FILE* out_;
};

// Quackle's answer as a move on this board ('.' for played-through squares).
Move ParseAnswer(const std::string& answer, const Game& game) {
  const std::string text =
      "#player1 q Quackle\n#player2 k Kingbble\n>q: " + game.rack(game.to_move()).ToString() + " " +
      answer + " +0 0\n";
  std::string error;
  auto parsed = ParseGcg(text, &error);
  if (!parsed || parsed->events.size() != 1)
    throw std::runtime_error("bad Quackle move '" + answer + "': " + error);
  Move m = parsed->events[0].move;
  if (m.kind == Move::Kind::kPlace) {
    const int dr = m.dir == Direction::kVertical ? 1 : 0;
    for (std::size_t k = 0; k < m.tiles.size(); ++k)
      if (!game.board().IsEmpty(m.row + dr * static_cast<int>(k),
                                m.col + (1 - dr) * static_cast<int>(k)))
        m.tiles[k] = '.';
  }
  return m;
}

GcgEvent Event(const Game& before, const Move& m, std::array<int, 2>& cumulative) {
  GcgEvent ev;
  ev.player = before.to_move();
  ev.rack = before.rack(ev.player).ToString();
  ev.move = m;
  if (m.kind == Move::Kind::kPlace) {
    ev.kind = GcgEvent::Kind::kPlace;
    ev.score = ScoreMove(before.board(), m);
  } else if (m.kind == Move::Kind::kExchange) {
    ev.kind = GcgEvent::Kind::kExchange;
    ev.exchanged = static_cast<int>(m.tiles.size());
  }
  cumulative[ev.player] += ev.score;
  ev.cumulative = cumulative[ev.player];
  return ev;
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, helper_path, bot_kind = "equity+peg+eg";
  std::string quackle_player = "Championship Player", quackle_lexicon = "nwl23", gcg_dir;
  int games = 10, quackle_threads = 0;
  std::uint64_t seed = 1;
  SimParams sim;
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
    } else if (a == "--helper") {
      helper_path = value();
    } else if (a == "--bot") {
      bot_kind = value();
    } else if (a == "--quackle-player") {
      quackle_player = value();
    } else if (a == "--quackle-lexicon") {
      quackle_lexicon = value();
    } else if (a == "--quackle-threads") {
      quackle_threads = std::stoi(value());
    } else if (a == "--gcg-dir") {
      gcg_dir = value();
    } else if (a == "--games") {
      games = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--threads") {
      sim.threads = std::stoi(value());
    } else if (a == "--sim-candidates") {
      sim.candidates = std::stoi(value());
    } else if (a == "--sim-iterations") {
      sim.max_iterations = std::stoi(value());
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
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE --leaves KLV --helper PATH [--bot KIND] [--games N]"
                   " [--seed S] [--quackle-player NAME] [--quackle-lexicon NAME]"
                   " [--quackle-threads N] [--gcg-dir DIR] [bot options of kingbble_selfplay]\n";
      return 2;
    }
  }
  if (lexicon.empty() || helper_path.empty()) {
    std::cerr << "--lexicon and --helper are required\n";
    return 2;
  }

  Gaddag gaddag = Gaddag::FromFile(lexicon);
  std::unique_ptr<Leaves> leaves;
  if (!leaves_path.empty()) leaves = std::make_unique<Leaves>(Leaves::FromFile(leaves_path));
  sim.seed = seed;
  std::unique_ptr<Bot> bot = MakeBot(bot_kind, gaddag, leaves.get(), sim, endgame_time, peg);
  std::vector<std::string> helper_args = {helper_path, "--lexicon", quackle_lexicon, "--player",
                                          quackle_player};
  if (quackle_threads > 0) {
    helper_args.push_back("--threads");
    helper_args.push_back(std::to_string(quackle_threads));
  }
  Helper quackle(helper_args);
  if (!gcg_dir.empty()) std::filesystem::create_directories(gcg_dir);

  double points = 0, spread_sum = 0, spread_sq = 0;
  for (int g = 0; g < games; ++g) {
    Game game(MixSeed(MixSeed(seed) + static_cast<std::uint64_t>(g)));
    const int seat = g % 2;  // Kingbble's seat
    GcgGame record;
    record.nicknames = seat == 0 ? std::array<std::string, 2>{"kingbble", "quackle"}
                                 : std::array<std::string, 2>{"quackle", "kingbble"};
    record.names = seat == 0 ? std::array<std::string, 2>{"Kingbble", "Quackle"}
                             : std::array<std::string, 2>{"Quackle", "Kingbble"};
    record.lexicon = quackle_lexicon;
    record.id = "seed " + std::to_string(seed) + " game " + std::to_string(g + 1);
    std::array<int, 2> cumulative{};
    while (!game.over()) {
      const Move m =
          game.to_move() == seat
              ? bot->Choose(game)
              : ParseAnswer(quackle.Ask(WriteGcg(record), game.rack(game.to_move())), game);
      record.events.push_back(Event(game, m, cumulative));
      game.Apply(m);  // throws on an illegal move
    }
    // End-of-game rack points and penalties.
    for (int p = 0; p < 2; ++p) {
      const int rest = game.score(p) - cumulative[p];
      if (rest == 0) continue;
      GcgEvent ev;
      ev.player = p;
      ev.kind = rest > 0 ? GcgEvent::Kind::kEndRackPoints : GcgEvent::Kind::kEndRackPenalty;
      ev.rack = game.rack(rest > 0 ? 1 - p : p).ToString();
      ev.score = rest;
      ev.cumulative = cumulative[p] += rest;
      record.events.push_back(ev);
    }
    const int spread = game.score(seat) - game.score(1 - seat);
    points += spread > 0 ? 1 : spread == 0 ? 0.5 : 0;
    spread_sum += spread;
    spread_sq += double(spread) * spread;
    std::cout << "game " << g + 1 << ": kingbble " << game.score(seat) << " - quackle "
              << game.score(1 - seat) << (seat == 0 ? " (kingbble first)" : "") << std::endl;
    if (!gcg_dir.empty())
      std::ofstream(gcg_dir + "/game" + std::to_string(g + 1) + ".gcg") << WriteGcg(record);
  }
  const double n = games, p = games ? points / n : 0;
  const double mean = games ? spread_sum / n : 0;
  const double sd = games > 1 ? std::sqrt((spread_sq - n * mean * mean) / (n - 1)) : 0;
  std::cout << "games: " << games << ", kingbble win %: " << 100 * p << " +- "
            << (games ? 100 * 1.96 * std::sqrt(p * (1 - p) / n) : 0)
            << " (95 % CI), mean spread: " << mean << " +- "
            << (games ? 1.96 * sd / std::sqrt(n) : 0) << "\n";
  return 0;
}
