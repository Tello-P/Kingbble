#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <string>
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

}  // namespace scrabble
