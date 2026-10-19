#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "scrabble/endgame.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/game.hpp"
#include "scrabble/leaves.hpp"

namespace scrabble {

struct ApproxPreEndgameParams {
  int candidates = 10;      // best plays by static equity
  int samples = 100;        // random deals of the unseen tiles, the same for every play
  int endgame_plies = 4;    // search depth of the endgame once the bag is empty
  double time_limit_s = 0;  // for the whole solve, 0 = none
  int threads = 1;
  std::uint64_t seed = 1;
};

struct ApproxPreEndgamePlay {
  Move move;
  double equity = 0;  // static equity
  double wins = 0;    // fraction of the samples won, ties counting half
  double spread = 0;  // mean final spread
  int samples = 0;
};

// Approximate pre-endgame for bags too large to solve exactly (3 to 6 tiles):
// for each of the best plays by static equity, the opponent's rack and the
// order of the bag are dealt at random from the tiles we cannot see (the same
// deals for every play), our play is made, both sides then play their best
// static-equity move until the bag is empty and the endgame is solved with a
// shallow search. Plays are ranked by wins, then by mean final spread.
class ApproxPreEndgameSolver {
 public:
  ApproxPreEndgameSolver(const Gaddag& gaddag, const Leaves& leaves);

  // Requires tiles in the bag. With a time limit, every play stops at the same
  // number of samples. Best play first.
  std::vector<ApproxPreEndgamePlay> Solve(const Game& game, const ApproxPreEndgameParams& params);

 private:
  struct Worker {
    Worker(const Gaddag& gaddag, const Leaves& leaves) : bot(gaddag, leaves), endgame(gaddag) {}
    EquityBot bot;
    EndgameSolver endgame;
  };

  // Our final spread after `move` in `deal`.
  int Playout(Worker& w, const Game& deal, const Move& move, int plies) const;

  const Gaddag& gaddag_;
  const Leaves& leaves_;
  std::vector<std::unique_ptr<Worker>> workers_;
};

}  // namespace scrabble
