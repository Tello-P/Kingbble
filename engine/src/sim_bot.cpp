#include "scrabble/sim_bot.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
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
  const Rack unseen = UnseenTiles(game.board(), game.rack(me));
  const int opp_size = unseen.Size() - game.bag_size();
  std::mt19937_64 rng(seed);
  std::array<Rack, 2> racks;
  racks[me] = game.rack(me);
  Rack rest = unseen;
  // Its own stream, so rejected racks leave the uniform deal unchanged.
  std::mt19937_64 pick(MixSeed(~seed));
  if (sampler_ && std::uniform_real_distribution<double>(0, 1)(pick) < params_.posterior_share) {
    // A rack that does not fit the position falls back to a uniform draw.
    std::optional<Rack> rack = sampler_->SampleRack(pick);
    if (rack && rack->Size() == opp_size && ContainsTiles(unseen, *rack)) {
      racks[1 - me] = *rack;
      rest = MinusTiles(unseen, *rack);
    }
  }
  std::vector<Tile> pool;
  for (int t = 0; t < kNumTileTypes; ++t) pool.insert(pool.end(), rest.Count(Tile(t)), Tile(t));
  std::shuffle(pool.begin(), pool.end(), rng);

  const std::size_t deal = static_cast<std::size_t>(opp_size - racks[1 - me].Size());
  Rack bag;
  for (std::size_t i = 0; i < pool.size(); ++i) (i < deal ? racks[1 - me] : bag).Add(pool[i]);
  return Game::FromPosition(game.board(), racks, {game.score(0), game.score(1)}, me, bag,
                            game.scoreless_turns(), MixSeed(seed));
}

void SimBot::set_reply_policy(const std::function<std::unique_ptr<ReplyPolicy>()>& make) {
  policies_.clear();
  if (make)
    for (std::size_t t = 0; t < bots_.size(); ++t) policies_.push_back(make());
}

Move SimBot::Reply(int thread, const Game& game, std::mt19937_64& rng) const {
  EquityBot& bot = *bots_[thread];
  if (!policies_.empty()) return policies_[thread]->Pick(game, rng);
  if (params_.reply_top <= 1 || params_.reply_temperature <= 0) return bot.Choose(game);
  std::vector<RankedMove> ranked = bot.Rank(game);
  const std::size_t k = std::min(ranked.size(), static_cast<std::size_t>(params_.reply_top));
  std::vector<double> weights(k);
  for (std::size_t i = 0; i < k; ++i)
    weights[i] = std::exp((ranked[i].equity - ranked[0].equity) / params_.reply_temperature);
  return ranked[std::discrete_distribution<std::size_t>(weights.begin(), weights.end())(rng)].move;
}

double SimBot::Playout(int thread, const Game& base, const Move& candidate,
                       std::uint64_t seed) const {
  EquityBot& bot = *bots_[thread];
  // The same stream for every candidate of an iteration.
  std::mt19937_64 rng(seed);
  Game g = base;
  const int me = g.to_move();
  const int before = g.score(me) - g.score(1 - me);
  g.Apply(candidate);
  double leave = 0;  // of the last mover, from our side
  for (int ply = 0; ply < params_.plies && !g.over(); ++ply) {
    const int mover = g.to_move();
    Move m = mover != me || params_.reply_own ? Reply(thread, g, rng) : bot.Choose(g);
    leave = 0;
    if (ply == params_.plies - 1 && g.bag_size() > 0) {
      leave = bot.leaves().Value(LeaveAfter(g.rack(mover), m));
      if (mover != me) leave = -leave;
    }
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

  const auto start = std::chrono::steady_clock::now();
  auto out_of_time = [&] {
    return params_.time_limit_s > 0 &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() >=
               params_.time_limit_s;
  };
  int done = 0;
  while (num_active > 1 && done < max_iterations &&
         !(done >= params_.min_iterations && out_of_time())) {
    const int end = std::min(max_iterations, done + params_.batch);
    std::atomic<int> next{done};
    auto work = [&](int thread) {
      for (int it; (it = next++) < end;) {
        const std::uint64_t seed = MixSeed(base_seed + static_cast<std::uint64_t>(it));
        Game pos = SamplePosition(game, seed);
        for (std::size_t c = 0; c < n; ++c)
          if (active[c])
            values[c][it] = Playout(thread, pos, ranked[c].move, MixSeed(seed ^ 0x9E9));
      }
    };
    std::vector<std::thread> threads;
    for (std::size_t t = 1; t < bots_.size(); ++t) threads.emplace_back(work, static_cast<int>(t));
    work(0);
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
                        se, iterations[c], !active[c],
                        std::vector<double>(values[c].begin(), values[c].begin() + iterations[c])});
  }
  // Survivors first, then by simulated value; stable keeps static order on ties.
  std::stable_sort(results_.begin(), results_.end(), [](const SimResult& a, const SimResult& b) {
    return a.pruned != b.pruned ? !a.pruned : a.mean > b.mean;
  });
  return results_.front().move;
}

}  // namespace scrabble
