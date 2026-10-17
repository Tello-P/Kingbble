#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/move.hpp"

namespace scrabble {

// One move line of a GCG file (poslfit format, as written by Quackle and Woogles).
struct GcgEvent {
  enum class Kind : std::uint8_t {
    kPlace,           // >p: RACK 8D WORD +s t
    kExchange,        // >p: RACK -ABC +0 t (tiles may be unknown: -3)
    kPass,            // >p: RACK - +0 t
    kPhonyWithdrawn,  // >p: RACK -- -s t (removes the previous placement)
    kChallengeBonus,  // >p: RACK (challenge) +s t
    kEndRackPoints,   // >p: (RACK) +s t (twice the opponent's leftover)
    kEndRackPenalty,  // >p: RACK (RACK) -s t (after six scoreless turns)
    kTimePenalty,     // >p: RACK (time) -s t
  };

  Kind kind = Kind::kPass;
  int player = 0;
  std::string rack;  // may be empty
  // Placement, exchange or pass. For placements, `tiles` is as written in the file:
  // played-through squares may be '.' or the letter on the board.
  Move move;
  int exchanged = 0;  // number of tiles exchanged, also when they are unknown
  int score = 0;      // signed
  int cumulative = 0;
  int line = 0;  // 1-based line number in the source
};

struct GcgGame {
  std::array<std::string, 2> nicknames;
  std::array<std::string, 2> names;
  std::string lexicon;
  std::string id;
  std::vector<GcgEvent> events;
};

// Parses GCG text. On failure returns nullopt and, if `error` is set, a message
// with the offending line.
std::optional<GcgGame> ParseGcg(std::string_view text, std::string* error = nullptr);
std::optional<GcgGame> ReadGcgFile(const std::string& path, std::string* error = nullptr);

// Writes a game as GCG text (placements use '.' for played-through squares).
std::string WriteGcg(const GcgGame& game);

// Replays a game on an empty board, checking tile positions, racks, every score
// with ScoreMove and the running totals.
struct GcgReplay {
  bool ok = false;
  std::string error;
  Board board;
  std::array<int, 2> scores{};
  // Placements normalized to '.' for played-through squares, with the file's
  // score; one entry per event that was replayed (passes and others included).
  std::vector<Move> moves;
  // Board before each replayed event, for position extraction.
  std::vector<Board> boards_before;
};

GcgReplay ReplayGcg(const GcgGame& game);

}  // namespace scrabble
