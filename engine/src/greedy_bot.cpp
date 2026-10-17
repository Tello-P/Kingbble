#include "scrabble/greedy_bot.hpp"

namespace scrabble {

Move GreedyBot::Choose(const Game& game) {
  const Rack& rack = game.rack(game.to_move());
  std::vector<Move> moves = gen_.Generate(game.board(), rack);
  last_num_moves_ = moves.size();
  const Move* best = nullptr;
  for (const Move& m : moves)
    if (!best || m.score > best->score) best = &m;
  if (best) return *best;

  Move m;
  if (game.bag_size() >= Game::kMinBagForExchange) {
    m.kind = Move::Kind::kExchange;
    m.tiles = rack.ToString();
  }
  return m;
}

}  // namespace scrabble
