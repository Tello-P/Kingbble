#pragma once

// Bots by name, shared by the self-play and match apps: "greedy", "equity" or
// "sim", with optional "+apeg", "+peg" and "+eg" suffixes.

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "scrabble/approx_preendgame_bot.hpp"
#include "scrabble/endgame_bot.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/greedy_bot.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/preendgame_bot.hpp"
#include "scrabble/sim_bot.hpp"

namespace scrabble {

struct PegOptions {
  int max_bag = 1;
  PreEndgameParams params;
  ApproxPreEndgameParams approx;
};

inline std::unique_ptr<Bot> MakeBot(const std::string& kind, const Gaddag& gaddag,
                                    const Leaves* leaves, const SimParams& sim, double endgame_time,
                                    const PegOptions& peg) {
  for (const std::string suffix : {"+eg", "+peg", "+apeg"}) {
    if (kind.size() <= suffix.size() || !kind.ends_with(suffix)) continue;
    auto inner = MakeBot(kind.substr(0, kind.size() - suffix.size()), gaddag, leaves, sim,
                         endgame_time, peg);
    if (suffix == "+eg")
      return std::make_unique<EndgameBot>(std::move(inner), gaddag, endgame_time);
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
  if ((kind == "equity" || kind == "sim") && !leaves) {
    std::cerr << "bot '" << kind << "' needs --leaves\n";
    std::exit(2);
  }
  if (kind == "equity") return std::make_unique<EquityBot>(gaddag, *leaves);
  if (kind == "sim") return std::make_unique<SimBot>(gaddag, *leaves, sim);
  std::cerr << "unknown bot: " << kind
            << " (greedy, equity, sim, optionally with +apeg, +peg and/or +eg)\n";
  std::exit(2);
}

}  // namespace scrabble
