#pragma once

#include <vector>

#include "scrabble/game.hpp"
#include "scrabble/sim_bot.hpp"

namespace scrabble {

struct WinParams {
  // Spread standard deviation of the rest of the game per tile left in the
  // bag: the playouts stop after three moves, so the final margin is the
  // simulated spread plus this noise.
  double spread_sd_per_tile = 10.0;
};

struct WinEstimate {
  double win = 0;        // probability of winning, ties count half
  double std_error = 0;  // standard error over the simulated values
};

// Win probability given the current lead of the player to move and the
// simulated spread of each iteration: the mean over iterations of
// P(lead + value + noise > 0), with normal noise of standard deviation
// `rest_sd`. With `rest_sd` 0 a margin of exactly 0 is a tie.
WinEstimate EstimateWin(const std::vector<double>& values, double lead, double rest_sd);

// Standard deviation of the spread still to come after the playouts.
double RestSpreadSd(int bag_size, const WinParams& params);

// Win estimate of every candidate of a SimBot's last results, in the same
// order, for the player to move in `game`.
std::vector<WinEstimate> WinProbabilities(const Game& game, const std::vector<SimResult>& results,
                                          const WinParams& params);

}  // namespace scrabble
