#include "scrabble/sim_bot.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <random>
#include <sstream>
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

constexpr int kWinSpread = 300;  // Macondo's MaxRepresentedWinSpread
constexpr int kWinUnseen = 94;   // columns 0..93

// Macondo's montecarlo stopping constants (stopping_condition.go, Stop99).
constexpr int kCheckInterval = 128;
constexpr int kIterationsCutoff = 2000;
constexpr int kPerPlyStopScaling = 625;
constexpr int kSimilarPlaysIterationsCutoff = 750;
constexpr int kMinIterationsForPruning = 128;
constexpr double kMinReasonableWProb = 0.005;
constexpr double kDelta = 0.01;
constexpr double kZ99 = 2.576;
constexpr double kWinPctSortEpsilon = 1e-9;

struct Running {  // mean and standard error, as Macondo's stats.Statistic
  double sum = 0, sq = 0;
  int n = 0;
  void Push(double x) {
    sum += x;
    sq += x * x;
    ++n;
  }
  double Mean() const { return n ? sum / n : 0; }
  double StdError() const {
    if (n < 2) return 0;
    const double var = std::max(0.0, (sq - sum * sum / n) / (n - 1));
    return std::sqrt(var / n);
  }
};

// The same multiset of tiles laid from the same square in the same direction.
bool MateriallySimilar(const Move& a, const Move& b) {
  if (a.kind != Move::Kind::kPlace || b.kind != Move::Kind::kPlace) return false;
  if (a.row != b.row || a.col != b.col || a.dir != b.dir) return false;
  if (a.TilesPlayed() != b.TilesPlayed() || a.tiles.size() != b.tiles.size()) return false;
  std::string x = a.tiles, y = b.tiles;
  std::sort(x.begin(), x.end());
  std::sort(y.begin(), y.end());
  return x == y;
}

bool WinIndistinguishable(double a, double b) {
  if (std::abs(a - b) <= kWinPctSortEpsilon) return true;
  return (a > 1 - kMinReasonableWProb && b > 1 - kMinReasonableWProb) ||
         (a < kMinReasonableWProb && b < kMinReasonableWProb);
}
}  // namespace

std::shared_ptr<const std::vector<float>> MacondoWinPctFromText(const std::string& text) {
  auto table = std::make_shared<std::vector<float>>((2 * kWinSpread + 1) * kWinUnseen, 0.f);
  std::istringstream in(text);
  std::string line;
  int row = -1;
  while (std::getline(in, line)) {
    if (line.empty() || line == "\r") continue;
    if (row++ < 0) continue;  // the header
    if (row > 2 * kWinSpread + 1) throw std::runtime_error("win table: too many rows");
    std::istringstream cells(line);
    std::string cell;
    std::getline(cells, cell, ',');  // the spread
    for (int u = 0; u < kWinUnseen; ++u) {
      if (!std::getline(cells, cell, ','))
        throw std::runtime_error("win table: short row " + std::to_string(row));
      (*table)[(row - 1) * kWinUnseen + u] = std::stof(cell);
    }
  }
  if (row != 2 * kWinSpread + 1) throw std::runtime_error("win table: missing rows");
  return table;
}

std::shared_ptr<const std::vector<float>> LoadMacondoWinPct(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::stringstream text;
  text << in.rdbuf();
  return MacondoWinPctFromText(text.str());
}

double MacondoPlayoutWin(const std::vector<float>& table, int spread, double leftover,
                         bool game_over, int unseen, bool plies_even) {
  if (game_over || unseen == 0) return spread > 0 ? 1.0 : spread == 0 ? 0.5 : 0.0;
  unseen = std::min(unseen, kWinUnseen - 1);
  int s = spread + static_cast<int>(std::lround(leftover));
  if (plies_even) s = -s;
  s = std::clamp(s, -kWinSpread, kWinSpread);
  double pct = table[(kWinSpread - s) * kWinUnseen + unseen];
  if (plies_even) pct = 1 - pct;
  return pct;
}

SimBot::SimBot(const Gaddag& gaddag, const Leaves& leaves, SimParams params) : params_(params) {
  params_.threads = std::max(1, params_.threads);
  params_.batch = std::max(1, params_.batch);
  for (int i = 0; i < params_.threads; ++i) {
    bots_.push_back(std::make_unique<EquityBot>(gaddag, leaves));
    bots_.back()->set_late_bag(params_.late_bag);
  }
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

double SimBot::Playout(int thread, const Game& base, const Move& candidate, std::uint64_t seed,
                       int plies) const {
  EquityBot& bot = *bots_[thread];
  // The same stream for every candidate of an iteration.
  std::mt19937_64 rng(seed);
  Game g = base;
  const int me = g.to_move();
  const int before = g.score(me) - g.score(1 - me);
  g.Apply(candidate);
  double leave = 0;  // of the last mover, from our side
  for (int ply = 0; ply < plies && !g.over(); ++ply) {
    const int mover = g.to_move();
    Move m = mover != me || params_.reply_own ? Reply(thread, g, rng) : bot.Choose(g);
    leave = 0;
    if (ply == plies - 1 && g.bag_size() > 0) {
      leave =
          LeaveValue(LeaveAfter(g.rack(mover), m), g.bag_size(), bot.leaves(), params_.late_bag);
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
  if (params_.macondo_style && params_.win_pct) return ChooseMacondo(game, ranked);
  const std::size_t n = std::min(ranked.size(), static_cast<std::size_t>(params_.candidates));
  const std::uint64_t base_seed = MixSeed(MixSeed(params_.seed) + calls_++);
  // With an empty bag the opponent's rack is known: one playout is exact.
  const int max_iterations = game.bag_size() == 0 ? 1 : std::max(1, params_.max_iterations);
  // Near the end, play every playout out: no game lasts this many more moves.
  const bool to_end =
      params_.end_unseen > 0 &&
      UnseenTiles(game.board(), game.rack(game.to_move())).Size() <= params_.end_unseen;
  const int plies = to_end ? 1000 : params_.plies;

  // Grown batch by batch: a time limit usually stops far from max_iterations.
  std::vector<std::vector<double>> values(n);
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
  const bool halving = params_.halving && params_.time_limit_s > 0;
  const int rounds =
      std::max(1, static_cast<int>(std::ceil(std::log2(std::max<std::size_t>(n, 2)))));
  int round = 1;
  int done = 0;
  while (num_active > 1 && done < max_iterations &&
         !(done >= params_.min_iterations && out_of_time())) {
    const int end = std::min(max_iterations, done + params_.batch);
    for (std::size_t c = 0; c < n; ++c)
      if (active[c]) values[c].resize(end);
    std::atomic<int> next{done};
    auto work = [&](int thread) {
      for (;;) {
        // A batch over many candidates can be long: the limit is checked at every
        // iteration. Those already taken finish, so the done ones stay contiguous.
        if (done >= params_.min_iterations && out_of_time()) break;
        const int it = next++;
        if (it >= end) break;
        const std::uint64_t seed = MixSeed(base_seed + static_cast<std::uint64_t>(it));
        Game pos = SamplePosition(game, seed);
        for (std::size_t c = 0; c < n; ++c)
          if (active[c])
            values[c][it] = Playout(thread, pos, ranked[c].move, MixSeed(seed ^ 0x9E9), plies);
      }
    };
    std::vector<std::thread> threads;
    for (std::size_t t = 1; t < bots_.size(); ++t) threads.emplace_back(work, static_cast<int>(t));
    work(0);
    for (auto& t : threads) t.join();
    const int reached = std::min(end, next.load());
    for (std::size_t c = 0; c < n; ++c)
      if (active[c]) iterations[c] = reached;
    done = reached;

    if (done < params_.min_iterations) continue;
    if (halving) {
      const double elapsed =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      if (elapsed < params_.time_limit_s * round / rounds) continue;
      ++round;
      std::vector<std::size_t> order;
      for (std::size_t c = 0; c < n; ++c)
        if (active[c]) order.push_back(c);
      std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return stats(a).first > stats(b).first;
      });
      for (std::size_t i = (order.size() + 1) / 2; i < order.size(); ++i) {
        active[order[i]] = 0;
        --num_active;
      }
      continue;
    }
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

// Macondo's simSingleIteration: after the candidate, `plies` moves, each the
// mover's best static move; the leaves of the last two (ours for us, theirs
// against) are the leftover. Equity is the spread gained plus the leftover;
// the win chance comes from the win table, or the result if the game is over.
SimBot::MacondoValue SimBot::PlayoutMacondo(int thread, const Game& base, const Move& candidate,
                                            std::uint64_t seed, int plies) const {
  EquityBot& bot = *bots_[thread];
  std::mt19937_64 rng(seed);
  Game g = base;
  const int me = g.to_move();
  const int before = g.score(me) - g.score(1 - me);
  g.Apply(candidate);
  double leftover = 0;
  for (int ply = 0; ply < plies && !g.over(); ++ply) {
    const int mover = g.to_move();
    Move m = mover != me ? Reply(thread, g, rng) : bot.Choose(g);
    if (ply >= plies - 2) {
      const double lv =
          LeaveValue(LeaveAfter(g.rack(mover), m), g.bag_size(), bot.leaves(), params_.late_bag);
      leftover += mover == me ? lv : -lv;
    }
    g.Apply(m);
  }
  const int spread = g.score(me) - g.score(1 - me);
  const int unseen = g.bag_size() + g.rack(1 - me).Size();
  return {spread - before + leftover,
          MacondoPlayoutWin(*params_.win_pct, spread, leftover, g.over(), unseen, plies % 2 == 0)};
}

// Macondo v0.13.9's SIMMING_BOT (ai/bot/elite.go, montecarlo/): every live
// candidate is played out on the same opponent rack each iteration; every 128
// iterations the Stop99 autostopper ranks them by win chance (equity breaks
// ties, and replaces win chance when the leader is all but lost or all but won)
// and drops each one whose upper bound falls below the leader's lower bound,
// c = sqrt(2 ln(K / 0.01)) standard errors with K live candidates, once it has
// 128 iterations; after 750 it also drops the ones materially like the leader.
// It stops with one candidate left, at 2000 + 625 x plies iterations, or when
// the time is up, and plays the live candidate with the best win chance.
Move SimBot::ChooseMacondo(const Game& game, const std::vector<RankedMove>& ranked) {
  const std::size_t n = std::min(ranked.size(), static_cast<std::size_t>(params_.candidates));
  const std::uint64_t base_seed = MixSeed(MixSeed(params_.seed) + calls_++);
  const int unseen = UnseenTiles(game.board(), game.rack(game.to_move())).Size();
  // Macondo sims 2 plies, and `unseen` plies with at most 14 unseen; A2's
  // end_unseen plays those out to the end instead.
  int plies = params_.plies, cap_plies = params_.plies;
  if (unseen <= 14) {
    cap_plies = unseen;
    plies = params_.end_unseen >= unseen ? 1000 : unseen;
  }
  const long cap = game.bag_size() == 0
                       ? 1
                       : kIterationsCutoff + static_cast<long>(kPerPlyStopScaling) * cap_plies;
  std::vector<Running> win(n), eq(n);
  std::vector<char> active(n, 1);
  std::size_t num_active = n;
  const auto start = std::chrono::steady_clock::now();
  auto out_of_time = [&] {
    return params_.time_limit_s > 0 &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() >=
               params_.time_limit_s;
  };
  long done = 0;
  std::vector<std::vector<MacondoValue>> chunk(n);
  while (num_active > 1 && done < cap && !out_of_time()) {
    const long end = std::min(cap, done + kCheckInterval);
    for (std::size_t c = 0; c < n; ++c)
      if (active[c]) chunk[c].assign(end - done, {});
    std::atomic<long> next{done};
    auto work = [&](int thread) {
      for (;;) {
        if (out_of_time()) break;
        const long it = next++;
        if (it >= end) break;
        const std::uint64_t seed = MixSeed(base_seed + static_cast<std::uint64_t>(it));
        Game pos = SamplePosition(game, seed);
        for (std::size_t c = 0; c < n; ++c)
          if (active[c])
            chunk[c][it - done] =
                PlayoutMacondo(thread, pos, ranked[c].move, MixSeed(seed ^ 0x9E9), plies);
      }
    };
    std::vector<std::thread> threads;
    for (std::size_t t = 1; t < bots_.size(); ++t) threads.emplace_back(work, static_cast<int>(t));
    work(0);
    for (auto& t : threads) t.join();
    const long reached = std::min(end, next.load());
    for (std::size_t c = 0; c < n; ++c)
      if (active[c])
        for (long i = 0; i < reached - done; ++i) {
          win[c].Push(chunk[c][i].win);
          eq[c].Push(chunk[c][i].equity);
        }
    done = reached;
    if (done < end) break;  // out of time

    // The autostopper.
    std::vector<std::size_t> order;
    for (std::size_t c = 0; c < n; ++c)
      if (active[c]) order.push_back(c);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
      const double wa = win[a].Mean(), wb = win[b].Mean();
      if (std::abs(wa - wb) <= kWinPctSortEpsilon) return eq[a].Mean() > eq[b].Mean();
      return wa > wb;
    });
    std::size_t leader = order.front();
    const std::size_t bottom = order.back();
    double mu = win[leader].Mean(), e = win[leader].StdError();
    auto z = [](double target, double mean, double se) { return (mean - target) / se; };
    bool by_equity = false;
    if (e > 0 && z(kMinReasonableWProb, mu, e) < -kZ99) {
      by_equity = true;
    } else if (e > 0 && win[bottom].StdError() > 0 && z(1 - kMinReasonableWProb, mu, e) > kZ99 &&
               z(1 - kMinReasonableWProb, win[bottom].Mean(), win[bottom].StdError()) > kZ99) {
      by_equity = true;
    }
    if (by_equity) {
      for (std::size_t c : order)
        if (eq[c].Mean() > eq[leader].Mean()) leader = c;
      mu = eq[leader].Mean();
      e = eq[leader].StdError();
    }
    const double k = std::max<double>(2, static_cast<double>(num_active));
    const double lcb = mu - std::sqrt(2 * std::log(k / kDelta)) * e;
    for (std::size_t c : order) {
      if (c == leader) continue;
      const Running& r = by_equity ? eq[c] : win[c];
      if (r.n < kMinIterationsForPruning) continue;
      const double ucb = r.Mean() + std::sqrt(2 * std::log(k / kDelta)) * r.StdError();
      if (lcb > ucb || (done > kSimilarPlaysIterationsCutoff &&
                        MateriallySimilar(ranked[leader].move, ranked[c].move))) {
        active[c] = 0;
        --num_active;
      }
    }
  }
  for (std::size_t c = 0; c < n; ++c)
    results_.push_back({ranked[c].move,
                        ranked[c].equity,
                        win[c].n ? win[c].Mean() : 0,
                        win[c].StdError(),
                        win[c].n,
                        !active[c],
                        {}});
  // Live candidates first, by win chance, equity breaking near ties.
  std::vector<std::size_t> idx(n);
  for (std::size_t c = 0; c < n; ++c) idx[c] = c;
  std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
    if (active[a] != active[b]) return active[a] > active[b];
    if (!WinIndistinguishable(win[a].Mean(), win[b].Mean())) return win[a].Mean() > win[b].Mean();
    return eq[a].Mean() > eq[b].Mean();
  });
  std::vector<SimResult> sorted;
  for (std::size_t c : idx) sorted.push_back(results_[c]);
  results_ = std::move(sorted);
  return results_.front().move;
}

}  // namespace scrabble
