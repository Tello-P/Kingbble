#include "scrabble/approx_preendgame.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <stdexcept>
#include <thread>

#include "scrabble/equity.hpp"

namespace scrabble {

namespace {

std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

double Now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The opponent's rack and the bag dealt at random from the tiles we cannot see.
Game Deal(const Game& game, std::uint64_t seed) {
  const int me = game.to_move();
  const Rack unseen = UnseenTiles(game.board(), game.rack(me));
  std::vector<Tile> pool;
  for (int t = 0; t < kNumTileTypes; ++t) pool.insert(pool.end(), unseen.Count(Tile(t)), Tile(t));
  std::mt19937_64 rng(seed);
  std::shuffle(pool.begin(), pool.end(), rng);
  const std::size_t opp_size = pool.size() - static_cast<std::size_t>(game.bag_size());
  std::array<Rack, 2> racks;
  racks[me] = game.rack(me);
  Rack bag;
  for (std::size_t i = 0; i < pool.size(); ++i) (i < opp_size ? racks[1 - me] : bag).Add(pool[i]);
  return Game::FromPosition(game.board(), racks, {game.score(0), game.score(1)}, me, bag,
                            game.scoreless_turns(), MixSeed(seed));
}

}  // namespace

ApproxPreEndgameSolver::ApproxPreEndgameSolver(const Gaddag& gaddag, const Leaves& leaves)
    : gaddag_(gaddag), leaves_(leaves) {}

int ApproxPreEndgameSolver::Playout(Worker& w, const Game& deal, const Move& move,
                                    int plies) const {
  Game g = deal;
  const int me = g.to_move();
  g.Apply(move);
  while (!g.over() && g.bag_size() > 0) g.Apply(w.bot.Choose(g));
  int spread = g.score(me) - g.score(1 - me);
  if (!g.over()) {
    const int v = w.endgame.Solve(g, plies).value;
    spread += g.to_move() == me ? v : -v;
  }
  return spread;
}

std::vector<ApproxPreEndgamePlay> ApproxPreEndgameSolver::Solve(
    const Game& game, const ApproxPreEndgameParams& params) {
  if (game.over() || game.bag_size() == 0)
    throw std::logic_error("approximate pre-endgame needs tiles in the bag");
  const int threads = std::max(1, params.threads);
  while (static_cast<int>(workers_.size()) < threads)
    workers_.push_back(std::make_unique<Worker>(gaddag_, leaves_));
  const double deadline = params.time_limit_s > 0 ? Now() + params.time_limit_s : 0;

  std::vector<RankedMove> ranked = workers_[0]->bot.Rank(game);
  ranked.resize(std::min(ranked.size(), static_cast<std::size_t>(std::max(1, params.candidates))));
  const std::size_t n = ranked.size();
  const int samples = std::max(1, params.samples);
  // spreads[s][c]: final spread of play c in deal s.
  std::vector<std::vector<int>> spreads(samples, std::vector<int>(n));
  std::vector<char> done(samples, 0);
  std::atomic<int> next{0};
  auto work = [&](Worker& w) {
    for (int s; (deadline == 0 || Now() < deadline) && (s = next++) < samples;) {
      const Game deal = Deal(game, MixSeed(params.seed + static_cast<std::uint64_t>(s)));
      for (std::size_t c = 0; c < n; ++c)
        spreads[s][c] = Playout(w, deal, ranked[c].move, params.endgame_plies);
      done[s] = 1;
    }
  };
  std::vector<std::thread> pool;
  for (int t = 1; t < threads; ++t) pool.emplace_back(work, std::ref(*workers_[t]));
  work(*workers_[0]);
  for (std::thread& t : pool) t.join();

  std::vector<ApproxPreEndgamePlay> plays(n);
  for (std::size_t c = 0; c < n; ++c) {
    ApproxPreEndgamePlay& p = plays[c];
    p.move = ranked[c].move;
    p.equity = ranked[c].equity;
    for (int s = 0; s < samples; ++s) {
      if (!done[s]) continue;
      ++p.samples;
      p.wins += spreads[s][c] > 0 ? 1 : spreads[s][c] == 0 ? 0.5 : 0;
      p.spread += spreads[s][c];
    }
    if (p.samples) {
      p.wins /= p.samples;
      p.spread /= p.samples;
    }
  }
  std::stable_sort(plays.begin(), plays.end(),
                   [](const ApproxPreEndgamePlay& a, const ApproxPreEndgamePlay& b) {
                     return a.wins != b.wins ? a.wins > b.wins : a.spread > b.spread;
                   });
  return plays;
}

}  // namespace scrabble
