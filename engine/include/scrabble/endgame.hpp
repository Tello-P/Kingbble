#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/game.hpp"
#include "scrabble/movegen.hpp"

namespace scrabble {

struct EndgameResult {
  Move move;
  // Spread the player to move gains from here to the end of the game with best
  // play by both sides (an estimate when `exact` is false).
  int value = 0;
  int depth = 0;  // plies of the last completed iteration
  bool exact = false;
  std::vector<Move> pv;
  long long nodes = 0;
};

// Exact endgame solver for an empty bag, where both racks are known: negamax
// with alpha-beta, iterative deepening, move ordering and a transposition
// table. As in Macondo's endgame mode, two consecutive scoreless turns end the
// game and each player loses their own rack value.
class EndgameSolver {
 public:
  explicit EndgameSolver(const Gaddag& gaddag);

  // Solve first prunes the lexicon to the words still playable with the tiles
  // left (on by default).
  void set_pruning(bool on) { pruning_ = on; }
  // Lexicon for SolveWindow, by default the full one. A pruned lexicon must
  // hold the words playable from every position later solved with it; set it
  // again whenever it changes.
  void set_window_gaddag(const Gaddag& gaddag) {
    window_gaddag_ = &gaddag;
    gen_on_window_ = false;
  }

  // Requires game.bag_size() == 0 and a game in progress. Stops at the first
  // exact iteration, at max_plies, or at the time limit (seconds, 0 = none);
  // depth 1 always completes.
  EndgameResult Solve(const Game& game, int max_plies = 25, double time_limit_s = 0);

  // Value for the player to move searched with the window (alpha, beta): exact
  // when strictly inside it, otherwise a bound on the side it falls. Unlike
  // Solve it keeps the transposition table of earlier calls (values are
  // relative to the position, so they stay valid); ClearTable() empties it.
  // With a time limit (seconds, 0 = none) check aborted() afterwards: the value
  // is then meaningless.
  int SolveWindow(const Game& game, int alpha, int beta, int max_plies = 25,
                  double time_limit_s = 0);
  bool aborted() const { return aborted_; }
  void ClearTable() { tt_.clear(); }

 private:
  struct Entry {
    int depth;  // remaining plies searched (kExactDepth if no line was cut)
    int value;
    std::uint8_t bound;  // kExactBound, kLower, kUpper
    Move best;
  };

  void Load(const Game& game);
  int Search(int depth, int alpha, int beta, bool& complete);
  void OrderedMoves(const Move* tt_move, std::vector<Move>& moves);
  int Play(const Move& m);  // returns points scored; updates hash, side and scoreless
  void Undo(const Move& m, int prev_scoreless, std::uint64_t prev_hash);
  std::uint64_t Hash() const;  // from scratch; hash_ is kept up to date incrementally

  const Gaddag& gaddag_;
  const Gaddag* window_gaddag_;
  Gaddag pruned_;
  bool pruning_ = true;
  bool gen_on_window_ = false;  // gen_ is set to *window_gaddag_
  MoveGenerator gen_;
  Board board_;
  std::array<Rack, 2> racks_;
  int to_move_ = 0;
  int scoreless_ = 0;
  std::uint64_t hash_ = 0;
  int ply_ = 0;
  std::vector<std::vector<Move>> move_lists_;  // by ply
  std::vector<std::pair<int, int>> order_;     // OrderedMoves scratch
  std::vector<Move> sorted_;
  std::unordered_map<std::uint64_t, Entry> tt_;
  std::uint64_t root_key_ = 0;
  Move root_best_;
  long long nodes_ = 0;
  bool aborted_ = false;
  double deadline_ = 0;  // seconds on the steady clock, 0 = none
};

}  // namespace scrabble
