#pragma once

#include <string>
#include <string_view>

#include "scrabble/board.hpp"
#include "scrabble/game.hpp"
#include "scrabble/rack.hpp"

namespace scrabble {

// Crossword Game Position (the position format read by Macondo):
//   <15 rows, '/'-separated, digits for empty runs, lowercase for blanks>
//   <rack to move>/<other rack> <score to move>/<other score> <scoreless turns> lex <name>;
std::string BoardToCgp(const Board& board);
std::string GameToCgp(const Game& game, std::string_view lexicon);

}  // namespace scrabble
