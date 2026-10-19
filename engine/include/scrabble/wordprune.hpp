#pragma once

#include <string>
#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/rack.hpp"

namespace scrabble {

// Words of the lexicon that some line of the board can still hold with tiles
// from `pool` (blanks stand for any letter), as Macondo's wordprune: the word
// matches the board's letters where it covers them, no tile touches either end
// and the pool supplies its empty squares, at least one. With `pool` holding
// every tile not on the board, any word a later move can form (cross-words
// included) is among them, since the board only gains tiles from the pool.
// Sorted.
std::vector<std::string> PlayableWords(const Gaddag& gaddag, const Board& board, const Rack& pool);

// GADDAG of PlayableWords: move generation with it gives the same moves as with
// the full lexicon on this board and on any board reached from it.
Gaddag PruneLexicon(const Gaddag& gaddag, const Board& board, const Rack& pool);

}  // namespace scrabble
