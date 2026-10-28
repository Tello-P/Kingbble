#pragma once

#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/move.hpp"
#include "scrabble/rack.hpp"

namespace scrabble {

// Tiles a player cannot see: everything not on the board or on `own` (the bag
// plus the opponent's rack).
Rack UnseenTiles(const Board& board, const Rack& own);

// Tiles kept on `rack` after playing or exchanging `move`.
Rack LeaveAfter(Rack rack, const Move& move);

// Every distinct exchange of 1..7 tiles from `rack` (legality depends on the bag).
std::vector<Move> GenerateExchanges(const Rack& rack);

// Value of keeping `leave` with `bag_size` tiles in the bag. With late_bag > 0
// and 1..late_bag tiles in the bag, the leave table's value is blended towards
// minus the tiles' face value as the bag empties (weight bag / (late_bag + 1)
// on the table): near the end, kept tiles are mostly points that may be stuck.
double LeaveValue(const Rack& leave, int bag_size, const Leaves& leaves, int late_bag = 0);

// Static equity: score plus the value of the leave while the bag has tiles.
// With an empty bag, going out earns twice the unseen tiles (the opponent's
// rack) and keeping tiles costs twice their value plus 10, as Macondo does.
// `move.score` must already hold the move's score.
double StaticEquity(const Move& move, const Rack& rack, int bag_size, const Rack& unseen,
                    const Leaves& leaves, int late_bag = 0);

}  // namespace scrabble
