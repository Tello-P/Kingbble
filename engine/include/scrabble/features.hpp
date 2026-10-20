#pragma once

#include <array>
#include <string>
#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/move.hpp"
#include "scrabble/movegen.hpp"
#include "scrabble/rack.hpp"

namespace scrabble {

// Features of one option for the human move model (a conditional logit over
// every legal option of a turn). Anything constant within a turn cancels out,
// so equity enters only as the gap to the best option.
enum MoveFeature : int {
  kFeatGap,       // best static equity minus this option's (>= 0)
  kFeatScore,     // points scored
  kFeatLeave,     // leave value (0 with an empty bag)
  kFeatLogRank,   // log(1 + rank by static equity), 0 for the best
  kFeatTiles,     // tiles placed or exchanged
  kFeatBingo,     // 1 if all seven tiles are placed
  kFeatExchange,  // 1 for an exchange
  kFeatPass,      // 1 for a pass
  kNumMoveFeatures,
};

const std::array<const char*, kNumMoveFeatures>& MoveFeatureNames();

struct Option {
  Move move;
  double equity = 0;
  std::array<float, kNumMoveFeatures> features{};
};

// Every legal option for `rack` (placements, exchanges when the bag holds at
// least seven tiles, and the pass), best static equity first, with features.
// `unseen` is what the mover cannot see (for the empty-bag equity).
std::vector<Option> RankOptions(MoveGenerator& gen, const Board& board, const Rack& rack,
                                int bag_size, const Rack& unseen, const Leaves& leaves);

// True if both moves put the same tiles on the same squares (one-tile plays
// match in either direction), exchange the same tiles, or both pass.
bool SamePlay(const Move& a, const Move& b);

// Index of `move` in `options`, or -1 if it is not there (e.g. a phony).
int FindOption(const std::vector<Option>& options, const Move& move);

// Tiles in the bag when `board` holds `on_board` tiles and both racks were filled.
int BagSizeFor(int on_board);
int TilesOnBoard(const Board& board);

// One turn of a GCG game with the mover's rack, replayed to the board it was
// played on. `turn` counts placements, exchanges and passes from 0, as the
// Python turn store does.
struct ReplayedTurn {
  int turn = 0;
  int player = 0;
  Rack rack;
  bool known_rack = false;
  Board board;  // before the move
  int bag_size = 0;
  Move move;  // normalized: '.' for played-through squares
  bool phony = false;
};

// Turns of a replayed game; empty if the replay failed.
std::vector<ReplayedTurn> ReplayTurns(const GcgGame& game, std::string* error = nullptr);

}  // namespace scrabble
