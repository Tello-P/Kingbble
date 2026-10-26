// Match against Macondo: Kingbble's bot (a) plays Macondo's simulating bot (b),
// run by scripts/macondo_match (built inside a Macondo checkout), with the same
// time per move and the same number of threads. Kingbble's Game keeps the bag;
// Macondo is sent the position as CGP with only its own rack. Games are paired:
// games 2k-1 and 2k share a deal with the seats swapped.
//
//   kingbble_macondo_match --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2
//       --helper build/macondo_match/kingbble_macondo --macondo-data build/macondo_match/data
//       --games 40 --seed 1 --time 5 --threads 3 [--bot sim+peg+eg] [--sim-candidates N]
//       [--sim-plies N] [--results FILE [--status]] [--progress]
// Each side gets --time seconds per move: our simulation, pre-endgame and endgame
// each stop at it, and Macondo's whole decision does. With --results FILE every
// finished game is appended to FILE ("game N: S1 - S2, a in seat K, a M moves T s,
// b M moves T s", as kingbble_selfplay) and a rerun skips the games already there;
// --status prints the summary of FILE and exits.

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "bot_factory.hpp"
#include "results_file.hpp"
#include "scrabble/cgp.hpp"
#include "scrabble/gcg.hpp"

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

  // Macondo's move for a CGP position, as written after "MOVE ".
  std::string Ask(const std::string& cgp) {
    fputs((cgp + "\n").c_str(), in_);
    fflush(in_);
    char buf[4096];
    for (;;) {
      if (!fgets(buf, sizeof buf, out_)) throw std::runtime_error("the Macondo helper stopped");
      std::string line = buf;
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
      if (line.rfind("MOVE ", 0) == 0) return line.substr(5);
    }
  }

 private:
  pid_t pid_;
  FILE* in_;
  FILE* out_;
};

// The position for the player to move, without the opponent's rack.
std::string Position(const Game& game, const std::string& lexicon) {
  const int p = game.to_move();
  return BoardToCgp(game.board()) + " " + game.rack(p).ToString() + "/ " +
         std::to_string(game.score(p)) + "/" + std::to_string(game.score(1 - p)) + " " +
         std::to_string(game.scoreless_turns()) + " lex " + lexicon + ";";
}

// Macondo's answer as a move on this board ('.' for played-through squares).
Move ParseAnswer(const std::string& answer, const Game& game) {
  const std::string text =
      "#player1 m Macondo\n#player2 k Kingbble\n>m: " + game.rack(game.to_move()).ToString() + " " +
      answer + " +0 0\n";
  std::string error;
  auto parsed = ParseGcg(text, &error);
  if (!parsed || parsed->events.size() != 1)
    throw std::runtime_error("bad Macondo move '" + answer + "': " + error);
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

// Win % with its 95 % CI over the finished pairs, the mean spread and the time
// per move of each side.
std::string Summary(const ResultsFile& results, int games) {
  std::vector<double> score(games + 1, -1), spread(games + 1, 0);
  int moves[2] = {0, 0};
  double seconds[2] = {0, 0};
  for (const std::string& line : results.lines()) {
    int g = 0, s0 = 0, s1 = 0, seat = 0, ma = 0, mb = 0;
    double ta = 0, tb = 0;
    const int read = std::sscanf(
        line.c_str(), "game %d: %d - %d, a in seat %d, a %d moves %lf s, b %d moves %lf s", &g, &s0,
        &s1, &seat, &ma, &ta, &mb, &tb);
    if (read < 4 || g < 1 || g > games) continue;
    const int a = seat == 1 ? s0 : s1, b = seat == 1 ? s1 : s0;
    score[g] = a > b ? 1 : a < b ? 0 : 0.5;
    spread[g] = a - b;
    if (read == 8) moves[0] += ma, moves[1] += mb, seconds[0] += ta, seconds[1] += tb;
  }
  double sum = 0, sq = 0, spread_sum = 0;
  int n = 0;
  for (int g = 1; g + 1 <= games; g += 2) {
    if (score[g] < 0 || score[g + 1] < 0) continue;
    const double p = (score[g] + score[g + 1]) / 2;
    sum += p;
    sq += p * p;
    spread_sum += (spread[g] + spread[g + 1]) / 2;
    ++n;
  }
  const double mean = n ? sum / n : 0;
  const double se = n > 1 ? std::sqrt(std::max(0.0, (sq - n * mean * mean) / (n - 1) / n)) : 0;
  char buf[320];
  std::snprintf(buf, sizeof buf,
                "games: %d of %d, kingbble win %%: %.1f (95 %% CI %.1f-%.1f), mean spread: %+.1f, "
                "s/move kingbble %.2f, macondo %.2f",
                2 * n, games, 100 * mean, 100 * (mean - 1.96 * se), 100 * (mean + 1.96 * se),
                n ? spread_sum / n : 0.0, moves[0] ? seconds[0] / moves[0] : 0.0,
                moves[1] ? seconds[1] / moves[1] : 0.0);
  return buf;
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, helper_path, macondo_data, bot_kind = "sim+peg+eg";
  std::string macondo_lexicon = "CSW24", results_path;
  int games = 40;
  bool status_only = false, progress = false;
  std::uint64_t seed = 1;
  double move_time = 5;
  SimParams sim;
  sim.candidates = 10;
  sim.max_iterations = 100000;  // the time is the limit
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
    } else if (a == "--macondo-data") {
      macondo_data = value();
    } else if (a == "--macondo-lexicon") {
      macondo_lexicon = value();
    } else if (a == "--bot") {
      bot_kind = value();
    } else if (a == "--games") {
      games = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--time") {
      move_time = std::stod(value());
    } else if (a == "--threads") {
      sim.threads = std::stoi(value());
    } else if (a == "--sim-candidates") {
      sim.candidates = std::stoi(value());
    } else if (a == "--sim-plies") {
      sim.plies = std::stoi(value());
    } else if (a == "--results") {
      results_path = value();
    } else if (a == "--status") {
      status_only = true;
    } else if (a == "--progress") {
      progress = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE --leaves KLV --helper PATH --macondo-data DIR [--games N]"
                   " [--seed S] [--time S] [--threads N] [--bot KIND] [--sim-candidates N]"
                   " [--sim-plies N] [--macondo-lexicon NAME] [--results FILE [--status]]"
                   " [--progress]\n";
      return 2;
    }
  }
  games += games % 2;  // whole pairs
  if (status_only) {
    std::cout << Summary(ResultsFile(results_path, true), games) << "\n";
    return 0;
  }
  if (lexicon.empty() || leaves_path.empty() || helper_path.empty() || macondo_data.empty()) {
    std::cerr << "--lexicon, --leaves, --helper and --macondo-data are required\n";
    return 2;
  }

  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  sim.seed = seed;
  sim.time_limit_s = move_time;
  PegOptions peg;
  peg.params.candidates = 10;
  peg.params.time_limit_s = move_time;
  peg.approx.time_limit_s = move_time;
  std::unique_ptr<Bot> bot = MakeBot(bot_kind, gaddag, &leaves, sim, move_time, peg);
  // Macondo's endgame solver uses every core it sees: cap Go at our threads.
  setenv("GOMAXPROCS", std::to_string(sim.threads).c_str(), 1);
  Helper macondo({helper_path, "-data", macondo_data, "-time", std::to_string(move_time),
                  "-threads", std::to_string(sim.threads)});

  ResultsFile results(results_path);
  auto report = [&] {
    int done = 0;
    for (int g = 1; g <= games; ++g) done += results.Done(g);
    if (progress) std::cerr << "progress: " << done << "/" << games << std::endl;
  };
  report();
  for (int g = 0; g < games; ++g) {
    if (results.Done(g + 1)) continue;
    Game game(MixSeed(MixSeed(seed) + static_cast<std::uint64_t>(g / 2)));
    const int seat_a = g % 2;  // Kingbble's seat
    int moves[2] = {0, 0};     // by side: kingbble, macondo
    double seconds[2] = {0, 0};
    while (!game.over()) {
      const int side = game.to_move() == seat_a ? 0 : 1;
      const auto t0 = std::chrono::steady_clock::now();
      const Move m = side == 0 ? bot->Choose(game)
                               : ParseAnswer(macondo.Ask(Position(game, macondo_lexicon)), game);
      seconds[side] += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      ++moves[side];
      game.Apply(m);  // throws on an illegal move
    }
    char timing[96];
    std::snprintf(timing, sizeof timing, ", a %d moves %.2f s, b %d moves %.2f s", moves[0],
                  seconds[0], moves[1], seconds[1]);
    const std::string line = "game " + std::to_string(g + 1) + ": " +
                             std::to_string(game.score(0)) + " - " + std::to_string(game.score(1)) +
                             ", a in seat " + std::to_string(seat_a + 1) + timing;
    results.Append(g + 1, line);
    std::cout << line << std::endl;
    report();
  }
  std::cout << Summary(results, games) << "\n";
  return 0;
}
