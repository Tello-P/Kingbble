// Dumps positions from greedy self-play as CGP lines, optionally followed by
// every legal placement our generator finds (for differential tests).
//
//   kingbble_positions --lexicon lexica/ENABLE.txt --count 10000 --seed 1 [--moves]
//
// With --moves each position line is followed by "<n>" and n lines
// "<coord> <word> (<score>)", where the word uses '.' for played-through squares.

#include <cstdlib>
#include <iostream>
#include <string>

#include "scrabble/cgp.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/greedy_bot.hpp"
#include "scrabble/movegen.hpp"

using namespace scrabble;

int main(int argc, char** argv) {
  std::string lexicon, lexicon_name = "ENABLE";
  int count = 100;
  std::uint64_t seed = 1;
  bool with_moves = false;
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
    } else if (a == "--name") {
      lexicon_name = value();
    } else if (a == "--count") {
      count = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--moves") {
      with_moves = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE [--name LEX] [--count N] [--seed S] [--moves]\n";
      return 2;
    }
  }
  if (lexicon.empty()) {
    std::cerr << "--lexicon is required\n";
    return 2;
  }

  Gaddag gaddag = Gaddag::FromFile(lexicon);
  GreedyBot bot(gaddag);
  MoveGenerator gen(gaddag);
  int written = 0;
  for (std::uint64_t g = 0; written < count; ++g) {
    Game game(seed * 1000003 + g);
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
