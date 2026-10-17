#include "scrabble/equity.hpp"

#include <cctype>

#include "scrabble/game.hpp"

namespace scrabble {

Rack UnseenTiles(const Board& board, const Rack& own) {
  std::array<int, kNumTileTypes> counts{};
  for (int t = 0; t < kNumTileTypes; ++t) counts[t] = EnglishTiles()[t].count - own.Count(Tile(t));
  for (int r = 0; r < kBoardDim; ++r) {
    for (int c = 0; c < kBoardDim; ++c) {
      if (board.IsEmpty(r, c)) continue;
      std::uint8_t cell = board.At(r, c);
      --counts[(cell & Board::kBlankBit) ? kBlank : cell];
    }
  }
  Rack unseen;
  for (int t = 0; t < kNumTileTypes; ++t)
    for (int n = 0; n < counts[t]; ++n) unseen.Add(Tile(t));
  return unseen;
}

Rack LeaveAfter(Rack rack, const Move& move) {
  switch (move.kind) {
    case Move::Kind::kPlace:
      for (char ch : move.tiles) {
        if (ch == '.') continue;
        rack.Remove(std::islower(static_cast<unsigned char>(ch)) ? kBlank : *TileFromChar(ch));
      }
      break;
    case Move::Kind::kExchange:
      for (char ch : move.tiles) rack.Remove(*TileFromChar(ch));
      break;
    case Move::Kind::kPass:
      break;
  }
  return rack;
}

std::vector<Move> GenerateExchanges(const Rack& rack) {
  std::vector<Move> out;
  std::string chosen;
  auto rec = [&](auto& self, int t) -> void {
    if (t == kNumTileTypes) {
      if (chosen.empty()) return;
      Move m;
      m.kind = Move::Kind::kExchange;
      m.tiles = chosen;
      out.push_back(std::move(m));
      return;
    }
    for (int n = 0; n <= rack.Count(Tile(t)); ++n) {
      chosen.append(n, TileToChar(Tile(t)));
      self(self, t + 1);
      chosen.resize(chosen.size() - n);
    }
  };
  rec(rec, 0);
  return out;
}

double StaticEquity(const Move& move, const Rack& rack, int bag_size, const Rack& unseen,
                    const Leaves& leaves) {
  Rack leave = LeaveAfter(rack, move);
  if (bag_size > 0) return move.score + leaves.Value(leave);
  if (leave.Empty()) return move.score + 2 * RackValue(unseen);
  return move.score - 2 * RackValue(leave) - 10;
}

}  // namespace scrabble
