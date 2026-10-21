#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/features.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/move.hpp"
#include "scrabble/movegen.hpp"
#include "scrabble/rack.hpp"

namespace scrabble {

// Multiset helpers for racks.
bool ContainsTiles(const Rack& rack, const Rack& tiles);
Rack PlusTiles(Rack a, const Rack& b);
Rack MinusTiles(Rack a, const Rack& b);  // b must be contained in a
Rack CommonTiles(const Rack& a, const Rack& b);
// `k` tiles drawn uniformly from `pool` (all of them if it holds fewer).
Rack DrawTiles(const Rack& pool, int k, std::mt19937_64& rng);
// Probability of drawing exactly `sub` when drawing sub.Size() tiles from `pool`.
double DrawProbability(const Rack& pool, const Rack& sub);
// Distinct racks of `k` tiles from `pool`, counted up to `cap`.
long long CountSubRacks(const Rack& pool, int k, long long cap);
void ForEachSubRack(const Rack& pool, int k, const std::function<void(const Rack&)>& fn);

// Tiles a move takes off the rack: placed tiles ('?' for a blank) or exchanged ones.
Rack MoveTiles(const Move& move);

// What the observer knows when the opponent moves.
struct Observation {
  Board board;  // before the move
  Move move;    // '.' for played-through squares
  int bag_size = 0;
  Rack own;           // the observer's rack
  Rack unseen;        // tiles the observer cannot see before the move (bag + opponent rack)
  int exchanged = 0;  // tiles exchanged, also when move.tiles does not list them
  int rating = 0;     // the opponent's rating, 0 if unknown

  // Size of the opponent's rack before the move.
  int RackSize() const;
};

// P(observed move | the opponent held `rack`). The rack contains the move's tiles.
class MoveModel {
 public:
  virtual ~MoveModel() = default;
  virtual double Likelihood(const Observation& obs, const Rack& rack) = 0;
};

// Shared by the models: ranks the options of `rack` from the mover's view.
class OptionRanker {
 public:
  OptionRanker(const Gaddag& gaddag, const Leaves& leaves) : gen_(gaddag), leaves_(leaves) {}
  std::vector<Option> Rank(const Observation& obs, const Rack& rack);
  // Indices of the options matching the observed move (one, or every exchange
  // of the observed size when its tiles are unknown); empty if none matches.
  static std::vector<int> Matches(const Observation& obs, const std::vector<Option>& options);
  const Leaves& leaves() const { return leaves_; }

 private:
  MoveGenerator gen_;
  const Leaves& leaves_;
};

// RangeFinder baseline (Quackle, Macondo): a rack explains the move if the move
// is within `tau` equity points of the rack's best option, and all such racks
// are equally likely.
class RangeFinderModel : public MoveModel {
 public:
  RangeFinderModel(const Gaddag& gaddag, const Leaves& leaves, double tau = 0)
      : ranker_(gaddag, leaves), tau_(tau) {}
  double Likelihood(const Observation& obs, const Rack& rack) override;

 private:
  OptionRanker ranker_;
  double tau_;
};

// Parameters of the human move model, as written by ml/human_model/logit.py:
//   top_k 64
//   epsilon 0.05
//   residual 0.03
//   band <min rating> <one weight per feature, in MoveFeatureNames() order>
// '#' starts a comment. A rating below every band (or 0, unknown) uses the first.
struct HumanModelParams {
  using Weights = std::array<double, kNumMoveFeatures>;

  int top_k = 64;
  double epsilon = 0.05;   // weight of the uniform component
  double residual = 0.03;  // human mass shared by the options outside the top_k
  std::vector<int> band_ratings{0};
  std::vector<Weights> band_weights{DefaultWeights()};

  // Hand-set weights: humans mostly play near the top by equity.
  static Weights DefaultWeights();
  // Throws std::runtime_error on a malformed file.
  static HumanModelParams FromFile(const std::string& path);
  const Weights& WeightsFor(int rating) const;
};

// Hierarchical mixture: P(move | rack) = (1 - epsilon) * human + epsilon * uniform
// over the n legal options. The human part is a conditional logit (softmax of
// the weighted features) over the top_k options by static equity, scaled by
// 1 - residual; the options outside the top_k share the residual, so a rare
// move is never impossible. A move we do not generate (a phony) counts as one
// more option outside the top_k.
class HumanMoveModel : public MoveModel {
 public:
  HumanMoveModel(const Gaddag& gaddag, const Leaves& leaves, HumanModelParams params)
      : ranker_(gaddag, leaves), params_(std::move(params)) {}
  double Likelihood(const Observation& obs, const Rack& rack) override;

  // Probability of each of the n options, then of an option outside them (n + 1 values).
  static std::vector<double> OptionProbabilities(const std::vector<Option>& options,
                                                 const HumanModelParams& params, int rating);
  const HumanModelParams& params() const { return params_; }

 private:
  OptionRanker ranker_;
  HumanModelParams params_;
};

// A weighted set of full racks the opponent may have held before the move.
struct Hypothesis {
  Rack rack;
  double weight = 0;
};

class RackPosterior {
 public:
  RackPosterior() = default;
  explicit RackPosterior(std::vector<Hypothesis> h) : hypotheses_(std::move(h)) { Normalize(); }

  const std::vector<Hypothesis>& hypotheses() const { return hypotheses_; }
  bool empty() const { return hypotheses_.empty(); }
  // Merges equal racks and rescales the weights to sum to 1 (uniform if they sum to 0).
  void Normalize();
  // Posterior probability of `rack`.
  double Probability(const Rack& rack) const;
  // Probability that the rack holds at least one `t`.
  double TileProbability(Tile t) const;
  const Rack& Map() const;  // most likely rack
  const Rack& Sample(std::mt19937_64& rng) const;
  // Same posterior over what is left after removing `tiles` from every rack.
  RackPosterior Without(const Rack& tiles) const;

 private:
  std::vector<Hypothesis> hypotheses_;
};

struct InferParams {
  int samples = 1000;            // sampled leaves when exact enumeration is too large
  long long exact_limit = 3000;  // enumerate every leave when there are at most this many
  std::uint64_t seed = 1;
};

// Posterior over the opponent's rack before an observed move: the move's tiles
// plus a leave drawn from the unseen tiles, weighted by `model`. With no rack
// explaining the move (e.g. a phony for the baseline) the prior is kept.
RackPosterior InferRack(const Observation& obs, MoveModel& model, const InferParams& params);

// A source of opponent racks for simulations.
class RackSampler {
 public:
  virtual ~RackSampler() = default;
  // A rack the opponent may hold now, or nullopt if there is none to offer.
  virtual std::optional<Rack> SampleRack(std::mt19937_64& rng) const = 0;
};

struct FilterParams {
  int particles = 500;
  long long exact_limit = 3000;  // track every rack exactly when there are at most this many
  double prior_mix = 0.1;        // prior share of each rack when switching to exact tracking
  double resample_below = 0.5;   // resample when the effective particles drop below this share
  std::uint64_t seed = 1;
};

// Tracks the opponent's current rack across turns. Each hypothesis is a full
// rack: an opponent move reweights it by the move model, then its kept tiles
// stay and the rest is drawn from the bag. Hypotheses that no longer fit what
// the observer sees are rejuvenated (their tiles that still fit are kept and
// the rest redrawn). With few possible racks the filter enumerates them all
// and updates exactly.
class RackFilter : public RackSampler {
 public:
  RackFilter(MoveModel& model, const Leaves& leaves, FilterParams params);

  // Prior: racks of `rack_size` tiles drawn from `unseen`.
  void Reset(const Rack& unseen, int rack_size);
  // The opponent played obs.move (obs.unseen is the observer's view before it).
  void ObserveOpponent(const Observation& obs);
  // The observer's view changed (their own move and draw): `unseen` is what they
  // cannot see now and `rack_size` the opponent's rack size.
  void Resync(const Rack& unseen, int rack_size);

  RackPosterior posterior() const { return RackPosterior(hypotheses_); }
  bool exact() const { return exact_; }
  const Rack& unseen() const { return unseen_; }
  std::optional<Rack> SampleRack(std::mt19937_64& rng) const override;

 private:
  double Likelihood(const Observation& obs, const Rack& rack);

  MoveModel& model_;
  const Leaves& leaves_;
  FilterParams params_;
  std::mt19937_64 rng_;
  std::vector<Hypothesis> hypotheses_;  // weights sum to 1
  bool exact_ = false;
  Rack unseen_;
  int rack_size_ = 0;
  std::unordered_map<std::string, double> cache_;  // likelihoods for the current move

  void SetExact(const Rack& unseen, int rack_size, double prior_mix);
  void Normalize();
  void MaybeResample();
  void Transition(const Observation& obs);
  // `rack` made to fit: `required` tiles, then its own tiles found in `pool`,
  // then random tiles from `pool`, `size` in all.
  Rack Rejuvenate(const Rack& rack, const Rack& required, const Rack& pool, int size);
};

}  // namespace scrabble
