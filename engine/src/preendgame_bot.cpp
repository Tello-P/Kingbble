#include "scrabble/preendgame_bot.hpp"

namespace scrabble {

Move PreEndgameBot::Choose(const Game& game) {
  last_ = PreEndgameResult{};
  const int bag = game.bag_size();
  if (bag >= 1 && bag <= max_bag_ && game.rack(game.to_move()).Size() == kRackSize) {
    last_ = solver_.Solve(game, params_);
    last_num_moves_ = last_.plays.size();
    // Plays the solver finished come first, best first.
    if (!last_.plays.empty() && !last_.plays[0].skipped && !last_.plays[0].cut)
      return last_.plays[0].move;
  }
  Move m = inner_->Choose(game);
  last_num_moves_ = inner_->last_num_moves();
  return m;
}

}  // namespace scrabble
