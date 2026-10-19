#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "scrabble/endgame.hpp"
#include "scrabble/game.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/movegen.hpp"

namespace scrabble {

// Ordered from best to worst for the player solving; kUnknown when a line was
// not followed to the end (see PreEndgameParams::nested_depth_limit).
enum class PegOutcome : std::uint8_t { kUnknown, kWin, kDraw, kLoss };

struct PreEndgameParams {
  int endgame_plies = 25;
  // Only the best plays by static equity are analysed (0 = all); passing too
  // when pass_candidate is set.
  int candidates = 0;
  bool pass_candidate = false;
  // Plays leaving more tiles than this in the bag are not analysed (-1 = all).
  int max_tiles_left = 1;
  // How many times our own turn with tiles still in the bag is solved as a
  // nested pre-endgame; deeper lines stay kUnknown (-1 = no limit).
  int nested_depth_limit = 1;
  // In nested pre-endgames we only consider passing right after a scoreless turn.
  bool skip_deep_pass = true;
  // Stop analysing a play once it has more losses than the best play could have.
  bool early_cutoff = true;
  // Order plays tied for the most wins that empty the bag by total spread.
  bool tiebreak = true;
  int tiebreak_plays = 20;
  double time_limit_s = 0;  // 0 = none
  // Plays and draws are solved by this many threads, each with its own
  // endgame solver. Only which plays are cut early can change with it.
  int threads = 1;
};

struct PreEndgamePlay {
  Move move;
  double equity = 0;  // static equity, the order in which plays are analysed
  // Counted in ordered draws of the bag tiles (permutations), each tied one
  // counting half on both sides.
  double wins = 0;
  double losses = 0;
  bool skipped = false;  // not a candidate or leaves more than max_tiles_left tiles in the bag
  bool cut = false;      // abandoned once it could no longer have the most wins
  bool has_spread = false;
  double spread = 0;  // tiebreak: final spread summed over every draw
};

struct PreEndgameResult {
  std::vector<PreEndgamePlay> plays;  // best first
  double permutations = 0;            // ordered draws of the bag from the unseen tiles
  long long endgames = 0;
  bool complete = true;  // false if the time limit stopped the search
};

// Exhaustive pre-endgame solver (1 to 6 tiles in the bag), following Macondo's.
// Every ordered draw of the bag from the tiles we cannot see is tried for each
// of our plays, the opponent holding the rest. Lines are followed until the bag
// is empty and the endgame is solved; only who wins matters. The opponent is
// assumed to find any reply that beats us (a draw is lost if one reply loses).
// When it is our turn again with tiles in the bag we do not know the draw:
// the position is solved as a nested pre-endgame over the tiles unseen to us
// and we take the worst outcome among the plays with the fewest losses.
// Plays are ranked by wins (ties counting half).
class PreEndgameSolver {
 public:
  PreEndgameSolver(const Gaddag& gaddag, const Leaves& leaves);

  // Solve first prunes the lexicon to the words still playable with the tiles
  // off the board (on by default).
  void set_pruning(bool on) { pruning_ = on; }

  // Requires 1..6 tiles in the bag and a full rack for the player to move; the
  // opponent's rack is ignored (it is part of the unseen tiles).
  PreEndgameResult Solve(const Game& game, const PreEndgameParams& params);

  // Ordered draws of k tiles from `pool`, each with the number of ways to
  // draw it when tiles of the same letter are told apart.
  struct Draw {
    std::array<Tile, kRackSize> tiles{};
    int size = 0;
    double count = 0;
  };
  static std::vector<Draw> Permutations(const Rack& pool, int k);

 private:
  struct State {
    Board board;
    std::array<Rack, 2> racks;
    std::array<Tile, kRackSize> bag{};  // drawn from front to back
    int bag_next = 0;
    int bag_size = 0;
    int spread = 0;  // solving player's score minus the opponent's
    int to_move = 0;
    int scoreless = 0;  // two consecutive scoreless turns end the game
    bool over = false;

    int BagLeft() const { return bag_size - bag_next; }
  };

  void Apply(State& s, const Move& m) const;
  void Deal(State& s, const Rack& unseen, const Draw& d) const;
  // How well we stand after our play: the spread less the opponent's best
  // reply. Draws are tried from the lowest, where losses show up first.
  int Hardness(const State& s);
  PegOutcome Continue(const State& s, int nested);
  PegOutcome Nested(const State& s, int nested);
  // Value of the endgame for the player to move searched with the window
  // (alpha, beta) as EndgameSolver::SolveWindow, through the shared cache.
  int EndgameValue(const State& s, int alpha, int beta);
  std::uint64_t EndgameKey(const State& s) const;
  PegOutcome Endgame(const State& s);
  int FinalSpread(const State& s);
  std::vector<Move> Replies(const State& s, bool with_pass);
  bool TimeUp();
  double TimeLeft() const;  // seconds, 0 = no limit

  const Gaddag& gaddag_;
  Gaddag pruned_;
  bool pruning_ = true;
  MoveGenerator gen_;
  const Leaves& leaves_;
  EndgameSolver endgame_;
  PreEndgameParams params_;
  int me_ = 0;
  long long endgames_ = 0;
  double deadline_ = 0;
  bool aborted_ = false;

  // Caches every worker of a solve shares.
  struct EndgameBounds {
    int lo = std::numeric_limits<int>::min();
    int hi = std::numeric_limits<int>::max();
  };
  struct Shared {
    std::mutex mutex;
    // Nested pre-endgames by information state: the outcome for each ordered bag.
    std::unordered_map<std::uint64_t, std::unordered_map<std::string, PegOutcome>> nested;
    // Bounds on the value of each endgame solved so far (for the player to
    // move), shared by every play and draw that reaches it.
    std::unordered_map<std::uint64_t, EndgameBounds> endgames;
  };
  std::unique_ptr<Shared> own_shared_;
  Shared* shared_;

  // This solver and threads - 1 helpers set up for the current solve.
  std::vector<PreEndgameSolver*> Workers(int threads, const Gaddag& lexicon);
  // Calls task(worker, i) for i in [0, n) on every worker's thread until the
  // time limit.
  static void RunTasks(const std::vector<PreEndgameSolver*>& workers, std::size_t n,
                       const std::function<void(PreEndgameSolver&, std::size_t)>& task);
  static bool Aborted(const std::vector<PreEndgameSolver*>& workers);
  std::vector<std::unique_ptr<PreEndgameSolver>> helpers_;
};

}  // namespace scrabble
