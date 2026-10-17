#include "scrabble/cgp.hpp"

namespace scrabble {

std::string BoardToCgp(const Board& board) {
  std::string out;
  for (int r = 0; r < kBoardDim; ++r) {
    if (r > 0) out += '/';
    int empty = 0;
    for (int c = 0; c < kBoardDim; ++c) {
      if (board.IsEmpty(r, c)) {
        ++empty;
        continue;
      }
      if (empty > 0) out += std::to_string(empty);
      empty = 0;
      std::uint8_t cell = board.At(r, c);
      char letter = TileToChar(cell & ~Board::kBlankBit);
      out += (cell & Board::kBlankBit) ? static_cast<char>(letter - 'A' + 'a') : letter;
    }
    if (empty > 0) out += std::to_string(empty);
  }
  return out;
}

std::string GameToCgp(const Game& game, std::string_view lexicon) {
  const int p = game.to_move();
  return BoardToCgp(game.board()) + " " + game.rack(p).ToString() + "/" +
         game.rack(1 - p).ToString() + " " + std::to_string(game.score(p)) + "/" +
         std::to_string(game.score(1 - p)) + " " + std::to_string(game.scoreless_turns()) +
         " lex " + std::string(lexicon) + ";";
}

}  // namespace scrabble
