#pragma once

// Bots by name, shared by the self-play and match apps: "greedy", "equity",
// "sim", "risk" (mean-variance), "riskwin" (max win %) or "human" (moves drawn
// from the human move model), with optional "+apeg", "+peg" and "+eg" suffixes.
// "sim+inf:MODEL" is a sim bot whose opponent racks come from a RackFilter on
// MODEL: "human" (the human move model), "rf" or "rangefinder", or "none"
// (uniform racks: the plain sim bot). It stacks with the other suffixes, e.g.
// "sim+inf:human+apeg+eg"; only MakeBotSetup builds it, since the game loop
// has to feed the tracker every move.

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "inference_side.hpp"
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
  // "+eg": the solver's depth limit and threads (0: the sim threads).
  int endgame_plies = 25;
  int endgame_threads = 0;
};

// The rack trackers of "+inf:" bots.
struct InferenceOptions {
  FilterParams filter;
  double tau = 0;  // RangeFinder temperature
};

// The human move model behind the "human" bot.
struct HumanBotOptions {
  const HumanModelParams* params = nullptr;
  const FeatureLexicon* lexicon = nullptr;  // for word features, may be null
  int rating = 0;                           // 0 if unknown
};

// A bot with the parts the game loop drives: the sim bot inside it (to reseed
// per game) and, for "+inf:" kinds, the tracker of the opponent's rack.
struct BotSetup {
  std::unique_ptr<Bot> bot;
  SimBot* sim = nullptr;
  std::unique_ptr<RackTracker> tracker;

  // Before each game, with this bot in `seat`: restarts the sim and tracker
  // streams from `seed` (if given), so the game's moves depend only on it.
  void NewGame(const Game& game, int seat, std::optional<std::uint64_t> seed = {}) {
    if (seed && sim) sim->Reseed(*seed);
    if (!tracker) return;
    if (seed) tracker->Reseed(*seed ^ 0x7ac4e7ULL);
    tracker->NewGame(game, seat);
  }
  void BeforeMove(const Game& game, const Move& m) {
    if (tracker) tracker->BeforeMove(game, m);
  }
  void AfterMove(const Game& game) {
    if (tracker) tracker->AfterMove(game);
  }
};

inline BotSetup MakeBotSetup(const std::string& kind, const Gaddag& gaddag, const Leaves* leaves,
                             const SimParams& sim, double endgame_time, const PegOptions& peg,
                             const RiskParams& risk = {}, const HumanBotOptions& human = {},
                             const InferenceOptions& inference = {}) {
  auto needs_leaves = [&] {
    if (!leaves) {
      std::cerr << "bot '" << kind << "' needs --leaves\n";
      std::exit(2);
    }
  };
  BotSetup out;
  for (const std::string suffix : {"+eg", "+peg", "+apeg"}) {
    if (kind.size() <= suffix.size() || !kind.ends_with(suffix)) continue;
    out = MakeBotSetup(kind.substr(0, kind.size() - suffix.size()), gaddag, leaves, sim,
                       endgame_time, peg, risk, human, inference);
    if (suffix == "+eg") {
      out.bot = std::make_unique<EndgameBot>(
          std::move(out.bot), gaddag, endgame_time,
          peg.endgame_threads > 0 ? peg.endgame_threads : sim.threads, peg.endgame_plies);
      return out;
    }
    needs_leaves();
    if (suffix == "+apeg") {
      ApproxPreEndgameParams approx = peg.approx;
      approx.threads = sim.threads;
      approx.seed = sim.seed;
      out.bot = std::make_unique<ApproxPreEndgameBot>(std::move(out.bot), gaddag, *leaves,
                                                      peg.max_bag + 1, 6, approx);
      return out;
    }
    out.bot = std::make_unique<PreEndgameBot>(std::move(out.bot), gaddag, *leaves, peg.max_bag,
                                              peg.params);
    return out;
  }
  if (const std::size_t inf = kind.find("+inf:"); inf != std::string::npos) {
    const std::string base = kind.substr(0, inf), model = kind.substr(inf + 5);
    if (base != "sim") {
      std::cerr << "bot '" << kind << "': \"+inf:\" only applies to sim\n";
      std::exit(2);
    }
    needs_leaves();
    auto bot = std::make_unique<SimBot>(gaddag, *leaves, sim);
    out.sim = bot.get();
    out.bot = std::move(bot);
    if (model == "none") return out;
    out.tracker = std::make_unique<RackTracker>(
        MakeMoveModel(model, gaddag, *leaves, human.params, human.lexicon, inference.tau), *leaves,
        inference.filter);
    out.tracker->Attach(out.sim);
    return out;
  }
  if (kind == "greedy") {
    out.bot = std::make_unique<GreedyBot>(gaddag);
    return out;
  }
  needs_leaves();
  if (kind == "equity") {
    out.bot = std::make_unique<EquityBot>(gaddag, *leaves);
  } else if (kind == "human") {
    if (!human.params) {
      std::cerr << "bot 'human' needs --human-params\n";
      std::exit(2);
    }
    out.bot = std::make_unique<HumanBot>(gaddag, *leaves, *human.params, human.lexicon,
                                         human.rating, sim.seed);
  } else if (kind == "sim") {
    auto bot = std::make_unique<SimBot>(gaddag, *leaves, sim);
    out.sim = bot.get();
    out.bot = std::move(bot);
  } else if (kind == "risk" || kind == "riskwin") {
    RiskParams params = risk;
    params.mode = kind == "risk" ? RiskMode::kMeanVariance : RiskMode::kWinProb;
    out.bot = std::make_unique<RiskBot>(gaddag, *leaves, sim, params);
  } else {
    std::cerr << "unknown bot: " << kind
              << " (greedy, equity, sim, sim+inf:MODEL, risk, riskwin, human, optionally with "
                 "+apeg, +peg and/or +eg)\n";
    std::exit(2);
  }
  return out;
}

// The bot alone, for apps that do not drive rack trackers.
inline std::unique_ptr<Bot> MakeBot(const std::string& kind, const Gaddag& gaddag,
                                    const Leaves* leaves, const SimParams& sim, double endgame_time,
                                    const PegOptions& peg, const RiskParams& risk = {},
                                    const HumanBotOptions& human = {}) {
  BotSetup setup = MakeBotSetup(kind, gaddag, leaves, sim, endgame_time, peg, risk, human);
  if (setup.tracker) {
    std::cerr << "bot '" << kind << "': rack inference is not supported here\n";
    std::exit(2);
  }
  return std::move(setup.bot);
}

}  // namespace scrabble
