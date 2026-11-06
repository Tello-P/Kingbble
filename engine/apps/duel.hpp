#pragma once

// Plays two moves out to the end of the game on the same deals, to see which one
// really wins more: both moves meet the same opponent racks and bags, and every
// later move is the mover's best by static equity.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/game.hpp"

namespace scrabble {

struct DuelResult {
  double win = 0;     // mean of (first move's points - second's), ties count half
  double win_se = 0;  // its standard error
  double spread = 0;  // mean of (first move's final spread - second's)
  double spread_se = 0;
  int rollouts = 0;
};

namespace duel {

inline Game Deal(const Game& game, std::uint64_t seed) {
  const int me = game.to_move();
  const Rack unseen = UnseenTiles(game.board(), game.rack(me));
  const int opp_size = unseen.Size() - game.bag_size();
  std::vector<Tile> pool;
  for (int t = 0; t < kNumTileTypes; ++t) pool.insert(pool.end(), unseen.Count(Tile(t)), Tile(t));
  std::mt19937_64 rng(seed);
  std::shuffle(pool.begin(), pool.end(), rng);
  std::array<Rack, 2> racks;
  racks[me] = game.rack(me);
  Rack bag;
  for (std::size_t i = 0; i < pool.size(); ++i)
    (i < static_cast<std::size_t>(opp_size) ? racks[1 - me] : bag).Add(pool[i]);
  return Game::FromPosition(game.board(), racks, {game.score(0), game.score(1)}, me, bag,
                            game.scoreless_turns(), seed);
}

// The mover's final spread after `move`, and in `points` 1, 0.5 or 0.
inline int Finish(Game game, const Move& move, EquityBot& bot, double* points) {
  const int me = game.to_move();
  game.Apply(move);
  while (!game.over()) game.Apply(bot.Choose(game));
  const int spread = game.score(me) - game.score(1 - me);
  *points = spread > 0 ? 1 : spread == 0 ? 0.5 : 0;
  return spread;
}

}  // namespace duel

// `first` and `second` are legal moves in `game`, whose opponent rack and bag are
// dealt anew from the unseen tiles for every rollout.
inline DuelResult Duel(const Game& game, const Move& first, const Move& second,
                       const Gaddag& gaddag, const Leaves& leaves, int rollouts, int threads,
                       std::uint64_t seed) {
  std::vector<double> wins(rollouts), spreads(rollouts);
  std::atomic<int> next{0};
  auto work = [&] {
    EquityBot bot(gaddag, leaves);
    for (int i; (i = next++) < rollouts;) {
      std::uint64_t s = seed + static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL;
      const Game dealt = duel::Deal(game, s);
      double a = 0, b = 0;
      const int spread_a = duel::Finish(dealt, first, bot, &a);
      const int spread_b = duel::Finish(dealt, second, bot, &b);
      wins[i] = a - b;
      spreads[i] = spread_a - spread_b;
    }
  };
  std::vector<std::thread> pool;
  for (int t = 1; t < threads; ++t) pool.emplace_back(work);
  work();
  for (std::thread& t : pool) t.join();
  auto summary = [&](const std::vector<double>& v, double* mean, double* se) {
    double sum = 0, sq = 0;
    for (double x : v) sum += x;
    *mean = v.empty() ? 0 : sum / v.size();
    for (double x : v) sq += (x - *mean) * (x - *mean);
    *se = v.size() > 1 ? std::sqrt(sq / (v.size() - 1) / v.size()) : 0;
  };
  DuelResult out;
  out.rollouts = rollouts;
  summary(wins, &out.win, &out.win_se);
  summary(spreads, &out.spread, &out.spread_se);
  return out;
}

}  // namespace scrabble
