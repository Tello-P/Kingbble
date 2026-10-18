#include "scrabble/endgame_bot.hpp"

namespace scrabble {

Move EndgameBot::Choose(const Game& game) {
  if (game.bag_size() > 0) {
    Move m = inner_->Choose(game);
    last_num_moves_ = inner_->last_num_moves();
    return m;
  }
  last_ = solver_.Solve(game, 25, time_limit_s_);
  last_num_moves_ = static_cast<std::size_t>(last_.nodes);
  return last_.move;
}

}  // namespace scrabble
