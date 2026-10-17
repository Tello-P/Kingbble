#include "scrabble/equity_bot.hpp"

#include "scrabble/equity.hpp"

namespace scrabble {

Move EquityBot::Choose(const Game& game) {
  const Rack& rack = game.rack(game.to_move());
  std::vector<Move> moves = gen_.Generate(game.board(), rack);
  if (game.bag_size() >= Game::kMinBagForExchange) {
    std::vector<Move> exchanges = GenerateExchanges(rack);
    moves.insert(moves.end(), exchanges.begin(), exchanges.end());
  }
  moves.emplace_back();  // pass
  last_num_moves_ = moves.size();

  Rack unseen = UnseenTiles(game.board(), rack);
  const Move* best = nullptr;
  double best_equity = 0;
  for (const Move& m : moves) {
    double e = StaticEquity(m, rack, game.bag_size(), unseen, leaves_);
    if (!best || e > best_equity || (e == best_equity && TiebreaksBetter(m, *best))) {
      best = &m;
      best_equity = e;
    }
  }
  return *best;
}

}  // namespace scrabble
