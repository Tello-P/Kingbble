#include "scrabble/equity_bot.hpp"

#include <algorithm>

#include "scrabble/equity.hpp"

namespace scrabble {

std::vector<Move> EquityBot::Options(const Game& game) {
  const Rack& rack = game.rack(game.to_move());
  std::vector<Move> moves = gen_.Generate(game.board(), rack);
  if (game.bag_size() >= Game::kMinBagForExchange) {
    std::vector<Move> exchanges = GenerateExchanges(rack);
    moves.insert(moves.end(), exchanges.begin(), exchanges.end());
  }
  moves.emplace_back();  // pass
  last_num_moves_ = moves.size();
  return moves;
}

Move EquityBot::Choose(const Game& game) {
  const Rack& rack = game.rack(game.to_move());
  std::vector<Move> moves = Options(game);
  Rack unseen = UnseenTiles(game.board(), rack);
  const Move* best = nullptr;
  double best_equity = 0;
  for (const Move& m : moves) {
    double e = StaticEquity(m, rack, game.bag_size(), unseen, leaves_, late_bag_);
    if (!best || e > best_equity || (e == best_equity && TiebreaksBetter(m, *best))) {
      best = &m;
      best_equity = e;
    }
  }
  return *best;
}

std::vector<RankedMove> EquityBot::Rank(const Game& game) {
  const Rack& rack = game.rack(game.to_move());
  Rack unseen = UnseenTiles(game.board(), rack);
  std::vector<RankedMove> ranked;
  for (Move& m : Options(game)) {
    double e = StaticEquity(m, rack, game.bag_size(), unseen, leaves_, late_bag_);
    ranked.push_back({std::move(m), e});
  }
  std::sort(ranked.begin(), ranked.end(), [](const RankedMove& a, const RankedMove& b) {
    return a.equity != b.equity ? a.equity > b.equity : TiebreaksBetter(a.move, b.move);
  });
  return ranked;
}

}  // namespace scrabble
