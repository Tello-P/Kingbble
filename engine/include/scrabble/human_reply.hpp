#pragma once

#include <cstdint>
#include <random>

#include "scrabble/bot.hpp"
#include "scrabble/features.hpp"
#include "scrabble/inference.hpp"
#include "scrabble/movegen.hpp"
#include "scrabble/sim_bot.hpp"

namespace scrabble {

// Replies inside SimBot's playouts drawn from the human move model: the mover's
// `top` best options by static equity, weighted by the model's probabilities
// (for a player of `rating`, 0 if unknown) and renormalized.
class HumanReplyPolicy : public ReplyPolicy {
 public:
  // `params` and `lexicon` (may be null) must outlive the policy.
  HumanReplyPolicy(const Gaddag& gaddag, const Leaves& leaves, const HumanModelParams& params,
                   const FeatureLexicon* lexicon, int rating = 0, int top = 32)
      : gen_(gaddag),
        leaves_(leaves),
        params_(params),
        features_(leaves, lexicon),
        rating_(rating),
        top_(top) {}

  Move Pick(const Game& game, std::mt19937_64& rng) override;

 private:
  MoveGenerator gen_;
  const Leaves& leaves_;
  const HumanModelParams& params_;
  FeatureComputer features_;
  int rating_;
  int top_;
};

// A player that moves as the human move model predicts: a simulated human
// opponent for self-play, drawing from HumanReplyPolicy's distribution.
class HumanBot : public Bot {
 public:
  // `params` and `lexicon` (may be null) must outlive the bot.
  HumanBot(const Gaddag& gaddag, const Leaves& leaves, const HumanModelParams& params,
           const FeatureLexicon* lexicon, int rating, std::uint64_t seed, int top = 32)
      : policy_(gaddag, leaves, params, lexicon, rating, top), rng_(seed) {}

  Move Choose(const Game& game) override { return policy_.Pick(game, rng_); }

 private:
  HumanReplyPolicy policy_;
  std::mt19937_64 rng_;
};

}  // namespace scrabble
