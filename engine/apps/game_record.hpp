#pragma once

// A game as GCG while it is played, shared by the match and self-play apps:
// every move with the mover's rack, then the end-of-game rack points or penalties.

#include <array>
#include <string>

#include "scrabble/game.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/movegen.hpp"

namespace scrabble {

class GameRecord {
 public:
  GameRecord(const std::array<std::string, 2>& nicknames, const std::array<std::string, 2>& names,
             const std::string& lexicon, const std::string& id) {
    gcg_.nicknames = nicknames;
    gcg_.names = names;
    gcg_.lexicon = lexicon;
    gcg_.id = id;
  }

  // `m` is about to be played in `before` (placements with '.' for played-through squares).
  void Add(const Game& before, const Move& m) {
    GcgEvent ev;
    ev.player = before.to_move();
    ev.rack = before.rack(ev.player).ToString();
    ev.move = m;
    if (m.kind == Move::Kind::kPlace) {
      ev.kind = GcgEvent::Kind::kPlace;
      ev.score = ScoreMove(before.board(), m);
    } else if (m.kind == Move::Kind::kExchange) {
      ev.kind = GcgEvent::Kind::kExchange;
      ev.exchanged = static_cast<int>(m.tiles.size());
    }
    cumulative_[ev.player] += ev.score;
    ev.cumulative = cumulative_[ev.player];
    gcg_.events.push_back(ev);
  }

  // End-of-game rack points and penalties of the finished `game`.
  void Finish(const Game& game) {
    for (int p = 0; p < 2; ++p) {
      const int rest = game.score(p) - cumulative_[p];
      if (rest == 0) continue;
      GcgEvent ev;
      ev.player = p;
      ev.kind = rest > 0 ? GcgEvent::Kind::kEndRackPoints : GcgEvent::Kind::kEndRackPenalty;
      ev.rack = game.rack(rest > 0 ? 1 - p : p).ToString();
      ev.score = rest;
      ev.cumulative = cumulative_[p] += rest;
      gcg_.events.push_back(ev);
    }
  }

  const GcgGame& gcg() const { return gcg_; }

 private:
  GcgGame gcg_;
  std::array<int, 2> cumulative_{};
};

}  // namespace scrabble
