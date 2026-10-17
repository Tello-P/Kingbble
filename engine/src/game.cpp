#include "scrabble/game.hpp"

#include <cctype>
#include <stdexcept>

#include "scrabble/movegen.hpp"

namespace scrabble {

int RackValue(const Rack& rack) {
  int v = 0;
  for (int t = 0; t < kNumTileTypes; ++t) v += rack.Count(Tile(t)) * EnglishTiles()[t].score;
  return v;
}

Game::Game(std::uint64_t seed) : bag_(seed) {
  bag_.Refill(racks_[0]);
  bag_.Refill(racks_[1]);
}

void Game::Apply(const Move& move) {
  if (over_) throw std::logic_error("game is over");
  Rack& rack = racks_[to_move_];
  int gained = 0;
  switch (move.kind) {
    case Move::Kind::kPlace: {
      gained = ScoreMove(board_, move);
      const int dr = move.dir == Direction::kVertical ? 1 : 0;
      const int dc = 1 - dr;
      for (std::size_t k = 0; k < move.tiles.size(); ++k) {
        char ch = move.tiles[k];
        if (ch == '.') continue;
        bool blank = std::islower(static_cast<unsigned char>(ch));
        Tile letter = *TileFromChar(ch);
        if (!rack.Remove(blank ? kBlank : letter)) throw std::logic_error("tile not on rack");
        board_.Place(move.row + dr * static_cast<int>(k), move.col + dc * static_cast<int>(k),
                     letter, blank);
      }
      bag_.Refill(rack);
      break;
    }
    case Move::Kind::kExchange: {
      if (bag_.Size() < kMinBagForExchange) throw std::logic_error("bag too small to exchange");
      Rack returned;
      for (char ch : move.tiles) {
        Tile t = *TileFromChar(ch);
        if (!rack.Remove(t)) throw std::logic_error("tile not on rack");
        returned.Add(t);
      }
      bag_.Refill(rack);
      for (int t = 0; t < kNumTileTypes; ++t)
        for (int n = 0; n < returned.Count(Tile(t)); ++n) bag_.Return(Tile(t));
      break;
    }
    case Move::Kind::kPass:
      break;
  }
  scores_[to_move_] += gained;
  scoreless_turns_ = gained > 0 ? 0 : scoreless_turns_ + 1;

  if (rack.Empty() && bag_.Empty()) {
    scores_[to_move_] += 2 * RackValue(racks_[1 - to_move_]);
    over_ = true;
  } else if (scoreless_turns_ >= kMaxScorelessTurns) {
    Finish();
  }
  to_move_ = 1 - to_move_;
}

void Game::Finish() {
  for (int p = 0; p < 2; ++p) scores_[p] -= RackValue(racks_[p]);
  over_ = true;
}

}  // namespace scrabble
