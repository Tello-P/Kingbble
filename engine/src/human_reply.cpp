#include "scrabble/human_reply.hpp"

#include <algorithm>

#include "scrabble/equity.hpp"

namespace scrabble {

Move HumanReplyPolicy::Pick(const Game& game, std::mt19937_64& rng) {
  const int me = game.to_move();
  const Rack& rack = game.rack(me);
  std::vector<Option> options = RankOptions(gen_, game.board(), rack, game.bag_size(),
                                            UnseenTiles(game.board(), rack), leaves_);
  options.resize(std::min(options.size(), static_cast<std::size_t>(std::max(1, top_))));
  TurnContext ctx;
  ctx.bag_size = game.bag_size();
  ctx.score_diff = game.score(me) - game.score(1 - me);
  ctx.rating = rating_;
  const int k = std::min(static_cast<int>(options.size()), params_.top_k);
  features_.Fill(game.board(), rack, ctx, options, k);
  // The last value is the mass of an option we do not generate: left out.
  std::vector<double> p = HumanMoveModel::OptionProbabilities(options, params_, rating_);
  p.resize(options.size());
  return options[std::discrete_distribution<std::size_t>(p.begin(), p.end())(rng)].move;
}

}  // namespace scrabble
