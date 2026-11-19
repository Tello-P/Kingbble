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

namespace {

// StaticEquity of a placement, from its score and leave.
struct PlacementEquity {
  int bag_size;
  const Leaves* leaves;
  int late_bag;
  double out_bonus;  // 2 x the unseen tiles' value: going out with an empty bag

  static double Eval(const void* context, int score, const Rack& leave) {
    const auto& e = *static_cast<const PlacementEquity*>(context);
    if (e.bag_size > 0) return score + LeaveValue(leave, e.bag_size, *e.leaves, e.late_bag);
    if (leave.Empty()) return score + e.out_bonus;
    return score - 2 * RackValue(leave) - 10;
  }
};

}  // namespace

// As the best of Options by StaticEquity, without building every placement.
Move EquityBot::Choose(const Game& game) {
  const Rack& rack = game.rack(game.to_move());
  const Rack unseen = UnseenTiles(game.board(), rack);
  const PlacementEquity context{game.bag_size(), &leaves_, late_bag_,
                                game.bag_size() > 0 ? 0.0 : 2.0 * RackValue(unseen)};
  Move best;
  double best_equity = 0;
  long n =
      gen_.GenerateBest(game.board(), rack, &PlacementEquity::Eval, &context, best, best_equity);
  bool found = n > 0;
  auto consider = [&](const Move& m) {
    const double e = StaticEquity(m, rack, game.bag_size(), unseen, leaves_, late_bag_);
    if (!found || e > best_equity || (e == best_equity && TiebreaksBetter(m, best))) {
      best = m;
      best_equity = e;
      found = true;
    }
  };
  if (game.bag_size() >= Game::kMinBagForExchange) {
    const std::vector<Move> exchanges = GenerateExchanges(rack);
    for (const Move& m : exchanges) consider(m);
    n += static_cast<long>(exchanges.size());
  }
  consider(Move{});  // pass
  last_num_moves_ = static_cast<std::size_t>(n + 1);
  return best;
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
