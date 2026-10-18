#pragma once

#include <memory>

#include "scrabble/bot.hpp"
#include "scrabble/endgame.hpp"

namespace scrabble {

// Plays like `inner` while the bag has tiles and solves the endgame once it is empty.
class EndgameBot : public Bot {
 public:
  EndgameBot(std::unique_ptr<Bot> inner, const Gaddag& gaddag, double time_limit_s)
      : inner_(std::move(inner)), solver_(gaddag), time_limit_s_(time_limit_s) {}

  Move Choose(const Game& game) override;
  const EndgameResult& last_result() const { return last_; }

 private:
  std::unique_ptr<Bot> inner_;
  EndgameSolver solver_;
  double time_limit_s_;
  EndgameResult last_;
};

}  // namespace scrabble
