#include "scrabble/approx_preendgame_bot.hpp"

namespace scrabble {

Move ApproxPreEndgameBot::Choose(const Game& game) {
  last_.clear();
  const int bag = game.bag_size();
  if (bag >= std::max(1, min_bag_) && bag <= max_bag_) {
    ApproxPreEndgameParams params = params_;
    params.seed = params_.seed + 1000003 * calls_++;
    last_ = solver_.Solve(game, params);
    if (!last_.empty() && last_[0].samples > 0) return last_[0].move;
  }
  Move m = inner_->Choose(game);
  last_num_moves_ = inner_->last_num_moves();
  return m;
}

}  // namespace scrabble
