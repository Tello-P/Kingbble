#include "scrabble/sim_bot.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <random>
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

}  // namespace

SimBot::SimBot(const Gaddag& gaddag, const Leaves& leaves, SimParams params) : params_(params) {
  params_.threads = std::max(1, params_.threads);
  params_.batch = std::max(1, params_.batch);
  for (int i = 0; i < params_.threads; ++i)
    bots_.push_back(std::make_unique<EquityBot>(gaddag, leaves));
}

Game SimBot::SamplePosition(const Game& game, std::uint64_t seed) const {
  const int me = game.to_move();
  Rack unseen = UnseenTiles(game.board(), game.rack(me));
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

double SimBot::Playout(EquityBot& bot, const Game& base, const Move& candidate) const {
  Game g = base;
  const int me = g.to_move();
  const int before = g.score(me) - g.score(1 - me);
  g.Apply(candidate);
  if (!g.over()) g.Apply(bot.Choose(g));  // opponent's reply
  double leave = 0;
  if (!g.over()) {  // our follow-up
    Move m = bot.Choose(g);
    if (g.bag_size() > 0) leave = bot.leaves().Value(LeaveAfter(g.rack(me), m));
    g.Apply(m);
    if (g.over()) leave = 0;
  }
  return g.score(me) - g.score(1 - me) - before + leave;
}

Move SimBot::Choose(const Game& game) {
  results_.clear();
  std::vector<RankedMove> ranked = bots_[0]->Rank(game);
  last_num_moves_ = bots_[0]->last_num_moves();
  const std::size_t n = std::min(ranked.size(), static_cast<std::size_t>(params_.candidates));
  const std::uint64_t base_seed = MixSeed(MixSeed(params_.seed) + calls_++);
  // With an empty bag the opponent's rack is known: one playout is exact.
  const int max_iterations = game.bag_size() == 0 ? 1 : std::max(1, params_.max_iterations);

  std::vector<std::vector<double>> values(n, std::vector<double>(max_iterations));
  std::vector<int> iterations(n, 0);
  std::vector<char> active(n, 1);
  std::size_t num_active = n;

  auto stats = [&](std::size_t c) {
    const int k = iterations[c];
    double sum = 0, sq = 0;
    for (int i = 0; i < k; ++i) sum += values[c][i];
    const double mean = k ? sum / k : 0;
    for (int i = 0; i < k; ++i) sq += (values[c][i] - mean) * (values[c][i] - mean);
    const double se = k > 1 ? std::sqrt(sq / (k - 1) / k) : 0;
    return std::make_pair(mean, se);
  };

  int done = 0;
  while (num_active > 1 && done < max_iterations) {
    const int end = std::min(max_iterations, done + params_.batch);
    std::atomic<int> next{done};
    auto work = [&](EquityBot& bot) {
      for (int it; (it = next++) < end;) {
        Game pos = SamplePosition(game, MixSeed(base_seed + static_cast<std::uint64_t>(it)));
        for (std::size_t c = 0; c < n; ++c)
          if (active[c]) values[c][it] = Playout(bot, pos, ranked[c].move);
      }
    };
    std::vector<std::thread> threads;
    for (std::size_t t = 1; t < bots_.size(); ++t) threads.emplace_back(work, std::ref(*bots_[t]));
    work(*bots_[0]);
    for (auto& t : threads) t.join();
    for (std::size_t c = 0; c < n; ++c)
      if (active[c]) iterations[c] = end;
    done = end;

    if (done < params_.min_iterations) continue;
    std::size_t best = n;
    for (std::size_t c = 0; c < n; ++c)
      if (active[c] && (best == n || stats(c).first > stats(best).first)) best = c;
    const auto [best_mean, best_se] = stats(best);
    for (std::size_t c = 0; c < n; ++c) {
      if (!active[c] || c == best) continue;
      const auto [mean, se] = stats(c);
      if (best_mean - mean > params_.prune_z * std::sqrt(best_se * best_se + se * se)) {
        active[c] = 0;
        --num_active;
      }
    }
  }

  for (std::size_t c = 0; c < n; ++c) {
    const auto [mean, se] = stats(c);
    results_.push_back({ranked[c].move, ranked[c].equity, iterations[c] ? mean : ranked[c].equity,
                        se, iterations[c], !active[c]});
  }
  // Survivors first, then by simulated value; stable keeps static order on ties.
  std::stable_sort(results_.begin(), results_.end(), [](const SimResult& a, const SimResult& b) {
    return a.pruned != b.pruned ? !a.pruned : a.mean > b.mean;
  });
  return results_.front().move;
}

}  // namespace scrabble
