#include "scrabble/inference.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace scrabble {

namespace {

double LogChoose(int n, int k) {
  if (k < 0 || k > n) return -INFINITY;
  return std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
}

}  // namespace

bool ContainsTiles(const Rack& rack, const Rack& tiles) {
  for (int t = 0; t < kNumTileTypes; ++t)
    if (tiles.Count(Tile(t)) > rack.Count(Tile(t))) return false;
  return true;
}

Rack PlusTiles(Rack a, const Rack& b) {
  for (int t = 0; t < kNumTileTypes; ++t)
    for (int n = 0; n < b.Count(Tile(t)); ++n) a.Add(Tile(t));
  return a;
}

Rack MinusTiles(Rack a, const Rack& b) {
  for (int t = 0; t < kNumTileTypes; ++t)
    for (int n = 0; n < b.Count(Tile(t)); ++n) a.Remove(Tile(t));
  return a;
}

Rack CommonTiles(const Rack& a, const Rack& b) {
  Rack r;
  for (int t = 0; t < kNumTileTypes; ++t)
    for (int n = 0; n < std::min(a.Count(Tile(t)), b.Count(Tile(t))); ++n) r.Add(Tile(t));
  return r;
}

Rack DrawTiles(const Rack& pool, int k, std::mt19937_64& rng) {
  std::vector<Tile> tiles;
  for (int t = 0; t < kNumTileTypes; ++t) tiles.insert(tiles.end(), pool.Count(Tile(t)), Tile(t));
  Rack out;
  k = std::min(k, static_cast<int>(tiles.size()));
  // Partial Fisher-Yates: the first k tiles of a uniform shuffle.
  for (int i = 0; i < k; ++i) {
    std::uniform_int_distribution<std::size_t> pick(i, tiles.size() - 1);
    std::swap(tiles[i], tiles[pick(rng)]);
    out.Add(tiles[i]);
  }
  return out;
}

double DrawProbability(const Rack& pool, const Rack& sub) {
  if (!ContainsTiles(pool, sub)) return 0;
  double log_p = -LogChoose(pool.Size(), sub.Size());
  for (int t = 0; t < kNumTileTypes; ++t)
    log_p += LogChoose(pool.Count(Tile(t)), sub.Count(Tile(t)));
  return std::exp(log_p);
}

long long CountSubRacks(const Rack& pool, int k, long long cap) {
  // ways[j]: distinct multisets of j tiles from the tile types seen so far.
  std::vector<long long> ways(k + 1, 0);
  ways[0] = 1;
  for (int t = 0; t < kNumTileTypes; ++t) {
    const int c = pool.Count(Tile(t));
    if (c == 0) continue;
    for (int j = k; j >= 1; --j)
      for (int m = 1; m <= std::min(c, j); ++m) ways[j] = std::min(cap, ways[j] + ways[j - m]);
  }
  return k < 0 ? 0 : ways[k];
}

void ForEachSubRack(const Rack& pool, int k, const std::function<void(const Rack&)>& fn) {
  Rack cur;
  auto rec = [&](auto& self, int t, int left) -> void {
    if (left == 0) {
      fn(cur);
      return;
    }
    if (t == kNumTileTypes) return;
    const int c = std::min(pool.Count(Tile(t)), left);
    for (int n = c; n >= 0; --n) {
      for (int i = 0; i < n; ++i) cur.Add(Tile(t));
      self(self, t + 1, left - n);
      for (int i = 0; i < n; ++i) cur.Remove(Tile(t));
    }
  };
  if (k >= 0 && k <= pool.Size()) rec(rec, 0, k);
}

Rack MoveTiles(const Move& move) {
  Rack r;
  if (move.kind == Move::Kind::kPass) return r;
  for (char c : move.tiles) {
    if (c == '.') continue;
    if (move.kind == Move::Kind::kPlace && std::islower(static_cast<unsigned char>(c))) {
      r.Add(kBlank);
    } else if (auto t = TileFromChar(c)) {
      r.Add(*t);
    }
  }
  return r;
}

int Observation::RackSize() const { return std::min(kRackSize, unseen.Size() - bag_size); }

std::vector<Option> OptionRanker::Rank(const Observation& obs, const Rack& rack) {
  // The mover cannot see their opponent's (the observer's) rack.
  const Rack mover_unseen = PlusTiles(MinusTiles(obs.unseen, rack), obs.own);
  return RankOptions(gen_, obs.board, rack, obs.bag_size, mover_unseen, leaves_);
}

std::vector<int> OptionRanker::Matches(const Observation& obs, const std::vector<Option>& options) {
  std::vector<int> out;
  if (obs.move.kind == Move::Kind::kExchange && obs.move.tiles.empty()) {
    for (std::size_t i = 0; i < options.size(); ++i)
      if (options[i].move.kind == Move::Kind::kExchange &&
          static_cast<int>(options[i].move.tiles.size()) == obs.exchanged)
        out.push_back(static_cast<int>(i));
    return out;
  }
  if (int i = FindOption(options, obs.move); i >= 0) out.push_back(i);
  return out;
}

double RangeFinderModel::Likelihood(const Observation& obs, const Rack& rack) {
  std::vector<Option> options = ranker_.Rank(obs, rack);
  const double best = options.front().equity;
  for (int i : OptionRanker::Matches(obs, options))
    if (options[i].equity >= best - tau_ - 1e-9) return 1;
  return 0;
}

HumanModelParams::Weights HumanModelParams::DefaultWeights() {
  Weights w{};
  w[kFeatGap] = -0.35;
  w[kFeatLogRank] = -0.5;
  return w;
}

HumanModelParams HumanModelParams::FromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  HumanModelParams p;
  p.band_ratings.clear();
  p.band_weights.clear();
  std::string line;
  for (int number = 1; std::getline(in, line); ++number) {
    line = line.substr(0, line.find('#'));
    std::istringstream ss(line);
    std::string key;
    if (!(ss >> key)) continue;
    bool ok = true;
    if (key == "top_k") {
      ok = static_cast<bool>(ss >> p.top_k) && p.top_k > 0;
    } else if (key == "epsilon") {
      ok = static_cast<bool>(ss >> p.epsilon) && p.epsilon >= 0 && p.epsilon <= 1;
    } else if (key == "residual") {
      ok = static_cast<bool>(ss >> p.residual) && p.residual >= 0 && p.residual < 1;
    } else if (key == "band") {
      int rating = 0;
      Weights w{};
      ok = static_cast<bool>(ss >> rating);
      for (double& x : w) ok = ok && static_cast<bool>(ss >> x);
      if (ok) {
        p.band_ratings.push_back(rating);
        p.band_weights.push_back(w);
      }
    } else {
      ok = false;
    }
    if (!ok) throw std::runtime_error(path + ":" + std::to_string(number) + ": bad line");
  }
  if (p.band_ratings.empty()) throw std::runtime_error(path + ": no band");
  for (std::size_t i = 1; i < p.band_ratings.size(); ++i)
    if (p.band_ratings[i] <= p.band_ratings[i - 1])
      throw std::runtime_error(path + ": bands must be sorted by rating");
  return p;
}

const HumanModelParams::Weights& HumanModelParams::WeightsFor(int rating) const {
  std::size_t band = 0;
  while (band + 1 < band_ratings.size() && rating >= band_ratings[band + 1]) ++band;
  return band_weights[band];
}

std::vector<double> HumanMoveModel::OptionProbabilities(const std::vector<Option>& options,
                                                        const HumanModelParams& params,
                                                        int rating) {
  const int n = static_cast<int>(options.size());
  const int k = std::min(n, params.top_k);
  const auto& w = params.WeightsFor(rating);
  std::vector<double> p(n + 1);
  double max_u = -INFINITY;
  for (int i = 0; i < k; ++i) {
    double u = 0;
    for (int f = 0; f < kNumMoveFeatures; ++f) u += w[f] * options[i].features[f];
    p[i] = u;
    max_u = std::max(max_u, u);
  }
  double z = 0;
  for (int i = 0; i < k; ++i) z += p[i] = std::exp(p[i] - max_u);
  const double outside = n - k + 1;  // options below the top_k and the unmatched slot
  const double uniform = params.epsilon / (n + 1);
  for (int i = 0; i < k; ++i)
    p[i] = (1 - params.epsilon) * (1 - params.residual) * p[i] / z + uniform;
  for (int i = k; i <= n; ++i) p[i] = (1 - params.epsilon) * params.residual / outside + uniform;
  return p;
}

double HumanMoveModel::Likelihood(const Observation& obs, const Rack& rack) {
  std::vector<Option> options = ranker_.Rank(obs, rack);
  std::vector<double> p = OptionProbabilities(options, params_, obs.rating);
  std::vector<int> matches = OptionRanker::Matches(obs, options);
  if (matches.empty()) return p.back();
  double sum = 0;
  for (int i : matches) sum += p[i];
  return sum;
}

void RackPosterior::Normalize() {
  std::unordered_map<std::string, std::size_t> index;
  std::vector<Hypothesis> merged;
  for (Hypothesis& h : hypotheses_) {
    auto [it, fresh] = index.emplace(h.rack.ToString(), merged.size());
    if (fresh) {
      merged.push_back(std::move(h));
    } else {
      merged[it->second].weight += h.weight;
    }
  }
  hypotheses_ = std::move(merged);
  double total = 0;
  for (const Hypothesis& h : hypotheses_) total += h.weight;
  for (Hypothesis& h : hypotheses_)
    h.weight = total > 0 ? h.weight / total : 1.0 / hypotheses_.size();
}

double RackPosterior::Probability(const Rack& rack) const {
  for (const Hypothesis& h : hypotheses_)
    if (h.rack == rack) return h.weight;
  return 0;
}

double RackPosterior::TileProbability(Tile t) const {
  double p = 0;
  for (const Hypothesis& h : hypotheses_)
    if (h.rack.Count(t) > 0) p += h.weight;
  return p;
}

const Rack& RackPosterior::Map() const {
  return std::max_element(
             hypotheses_.begin(), hypotheses_.end(),
             [](const Hypothesis& a, const Hypothesis& b) { return a.weight < b.weight; })
      ->rack;
}

const Rack& RackPosterior::Sample(std::mt19937_64& rng) const {
  double u = std::uniform_real_distribution<double>(0, 1)(rng);
  for (const Hypothesis& h : hypotheses_) {
    if (u < h.weight) return h.rack;
    u -= h.weight;
  }
  return hypotheses_.back().rack;
}

RackPosterior RackPosterior::Without(const Rack& tiles) const {
  std::vector<Hypothesis> out;
  for (const Hypothesis& h : hypotheses_)
    if (ContainsTiles(h.rack, tiles)) out.push_back({MinusTiles(h.rack, tiles), h.weight});
  return RackPosterior(std::move(out));
}

RackPosterior InferRack(const Observation& obs, MoveModel& model, const InferParams& params) {
  const Rack played = MoveTiles(obs.move);
  if (!ContainsTiles(obs.unseen, played)) return {};
  const Rack pool = MinusTiles(obs.unseen, played);
  const int leave_size = std::max(0, obs.RackSize() - played.Size());

  std::vector<Hypothesis> prior;  // leaves with their prior weight
  if (CountSubRacks(pool, leave_size, params.exact_limit + 1) <= params.exact_limit) {
    ForEachSubRack(pool, leave_size, [&](const Rack& leave) {
      prior.push_back({leave, DrawProbability(pool, leave)});
    });
  } else {
    std::mt19937_64 rng(params.seed);
    for (int i = 0; i < params.samples; ++i) prior.push_back({DrawTiles(pool, leave_size, rng), 1});
    prior = RackPosterior(std::move(prior)).hypotheses();  // merges repeated draws
  }
  std::vector<Hypothesis> post;
  double total = 0;
  for (const Hypothesis& h : prior) {
    Rack rack = PlusTiles(h.rack, played);
    const double w = h.weight * model.Likelihood(obs, rack);
    total += w;
    post.push_back({std::move(rack), w});
  }
  if (total <= 0)
    for (std::size_t i = 0; i < post.size(); ++i) post[i].weight = prior[i].weight;
  return RackPosterior(std::move(post));
}

}  // namespace scrabble
