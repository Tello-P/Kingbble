#pragma once

// Bots by name, shared by the self-play and match apps: "greedy", "equity",
// "sim", "risk" (mean-variance), "riskwin" (max win %) or "human" (moves drawn
// from the human move model), with optional "+apeg", "+peg" and "+eg" suffixes.

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "scrabble/approx_preendgame_bot.hpp"
#include "scrabble/endgame_bot.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/greedy_bot.hpp"
#include "scrabble/human_reply.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/preendgame_bot.hpp"
#include "scrabble/risk_bot.hpp"
#include "scrabble/sim_bot.hpp"

namespace scrabble {

struct PegOptions {
  int max_bag = 1;
  PreEndgameParams params;
  ApproxPreEndgameParams approx;
};

// The human move model behind the "human" bot.
struct HumanBotOptions {
  const HumanModelParams* params = nullptr;
  const FeatureLexicon* lexicon = nullptr;  // for word features, may be null
  int rating = 0;                           // 0 if unknown
};

inline std::unique_ptr<Bot> MakeBot(const std::string& kind, const Gaddag& gaddag,
                                    const Leaves* leaves, const SimParams& sim, double endgame_time,
                                    const PegOptions& peg, const RiskParams& risk = {},
                                    const HumanBotOptions& human = {}) {
  for (const std::string suffix : {"+eg", "+peg", "+apeg"}) {
    if (kind.size() <= suffix.size() || !kind.ends_with(suffix)) continue;
    auto inner = MakeBot(kind.substr(0, kind.size() - suffix.size()), gaddag, leaves, sim,
                         endgame_time, peg, risk, human);
    if (suffix == "+eg")
      return std::make_unique<EndgameBot>(std::move(inner), gaddag, endgame_time, sim.threads);
    if (!leaves) {
      std::cerr << "bot '" << kind << "' needs --leaves\n";
      std::exit(2);
    }
    if (suffix == "+apeg") {
      ApproxPreEndgameParams approx = peg.approx;
      approx.threads = sim.threads;
      approx.seed = sim.seed;
      return std::make_unique<ApproxPreEndgameBot>(std::move(inner), gaddag, *leaves,
                                                   peg.max_bag + 1, 6, approx);
    }
    return std::make_unique<PreEndgameBot>(std::move(inner), gaddag, *leaves, peg.max_bag,
                                           peg.params);
  }
  if (kind == "greedy") return std::make_unique<GreedyBot>(gaddag);
  if (kind != "greedy" && !leaves) {
    std::cerr << "bot '" << kind << "' needs --leaves\n";
    std::exit(2);
  }
  if (kind == "equity") return std::make_unique<EquityBot>(gaddag, *leaves);
  if (kind == "human") {
    if (!human.params) {
      std::cerr << "bot 'human' needs --human-params\n";
      std::exit(2);
    }
    return std::make_unique<HumanBot>(gaddag, *leaves, *human.params, human.lexicon, human.rating,
                                      sim.seed);
  }
  if (kind == "sim") return std::make_unique<SimBot>(gaddag, *leaves, sim);
  if (kind == "risk" || kind == "riskwin") {
    RiskParams params = risk;
    params.mode = kind == "risk" ? RiskMode::kMeanVariance : RiskMode::kWinProb;
    return std::make_unique<RiskBot>(gaddag, *leaves, sim, params);
  }
  std::cerr
      << "unknown bot: " << kind
      << " (greedy, equity, sim, risk, riskwin, human, optionally with +apeg, +peg and/or +eg)\n";
  std::exit(2);
}

}  // namespace scrabble
