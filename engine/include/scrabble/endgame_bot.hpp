#pragma once

#include <memory>

#include "scrabble/bot.hpp"
#include "scrabble/endgame.hpp"

namespace scrabble {

// Plays like `inner` while the bag has tiles and solves the endgame once it is
// empty, to at most `max_plies` plies within `time_limit_s` seconds (0: no
// limit). With no time limit and one thread the moves do not depend on the
// machine's speed.
class EndgameBot : public Bot {
 public:
  EndgameBot(std::unique_ptr<Bot> inner, const Gaddag& gaddag, double time_limit_s, int threads = 1,
             int max_plies = 25)
      : inner_(std::move(inner)),
        solver_(gaddag),
        time_limit_s_(time_limit_s),
        max_plies_(max_plies) {
    solver_.set_threads(threads);
  }

  Move Choose(const Game& game) override;
  const EndgameResult& last_result() const { return last_; }

  // Over every endgame solved so far: how many, how many were solved exactly
  // within the time limit, and how many times the solver's move differs from
  // the one `inner` would have played (only counted with compare_inner).
  struct Stats {
    long long solves = 0, exact = 0, compared = 0, changed = 0;
    long long depth_sum = 0;
  };
  const Stats& stats() const { return stats_; }
  void set_compare_inner(bool on) { compare_inner_ = on; }
  // Plays `inner`'s move when the solver runs out of time before an exact
  // solution (its depth-limited value only counts the racks left at the horizon).
  void set_inexact_fallback(bool on) { inexact_fallback_ = on; }

 private:
  std::unique_ptr<Bot> inner_;
  EndgameSolver solver_;
  double time_limit_s_;
  int max_plies_;
  EndgameResult last_;
  Stats stats_;
  bool compare_inner_ = false;
  bool inexact_fallback_ = false;
};

}  // namespace scrabble
