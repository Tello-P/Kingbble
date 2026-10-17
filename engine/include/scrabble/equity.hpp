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

// Static equity: score plus the value of the leave while the bag has tiles.
// With an empty bag, going out earns twice the unseen tiles (the opponent's
// rack) and keeping tiles costs twice their value plus 10, as Macondo does.
// `move.score` must already hold the move's score.
double StaticEquity(const Move& move, const Rack& rack, int bag_size, const Rack& unseen,
                    const Leaves& leaves);

}  // namespace scrabble
