#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/move.hpp"
#include "scrabble/movegen.hpp"
#include "scrabble/rack.hpp"

namespace scrabble {

// Features of one option for the human move model (a conditional logit or a
// small network over the top options of a turn). Anything constant within a
// turn cancels out of a logit, so equity enters as the gap to the best option
// and turn-level facts (context, missed bingos) mostly through interactions.
// RankOptions fills the base features of every option; FeatureComputer::Fill
// the rest for the options the model looks at. The names (MoveFeatureNames())
// are the contract with ml/data/features.py and the model files.
enum MoveFeature : int {
  // Base.
  kFeatGap,       // best static equity minus this option's (>= 0)
  kFeatScore,     // points scored
  kFeatLeave,     // leave value (0 with an empty bag)
  kFeatLogRank,   // log(1 + rank by static equity), 0 for the best
  kFeatTiles,     // tiles placed or exchanged
  kFeatBingo,     // 1 if all seven tiles are placed
  kFeatExchange,  // 1 for an exchange
  kFeatPass,      // 1 for a pass
  // A. The main word (0 for exchanges and passes).
  kFeatWordZipf,        // English frequency (wordfreq Zipf scale), 0 if unknown
  kFeatWordPlays,       // log(1 + times played in the train games)
  kFeatWordUnfamiliar,  // 1 if the word is neither in the frequency list nor played
  kFeatWordLength,
  kFeatWordAlt,       // 1 if the word is also in the second (American) list
  kFeatWordLogProb,   // log probability of drawing its letters, per letter
  kFeatRackAnagrams,  // log(1 + seven-letter anagrams of the rack)
  // B. Missed bingos.
  kFeatHadBingo,      // some option is a bingo
  kFeatBingoCount,    // log(1 + bingo options)
  kFeatBingoEasiest,  // highest Zipf frequency of a bingo word
  kFeatBingoGap,      // best bingo equity minus this option's (0 without bingos)
  kFeatMissedBingo,   // a bingo was available and this option is not one
  // C. The leave.
  kFeatLeaveSize,
  kFeatLeaveVowels,
  kFeatLeaveConsonants,
  kFeatLeaveDuplicates,  // tiles beyond the first of each letter
  kFeatLeaveS,
  kFeatLeaveBlank,
  kFeatLeaveQNoU,
  kFeatLeaveSynergy,    // sum of pair leave values minus their single values
  kFeatLeaveBingoProb,  // chance of a seven-letter bingo after drawing to seven
  kFeatLeaveCount0,     // 27 counts: blank, then A..Z
  kFeatLeaveCountLast = kFeatLeaveCount0 + kNumTileTypes - 1,
  // D. Shape on the board.
  kFeatOnDL,
  kFeatOnTL,
  kFeatOnDW,
  kFeatOnTW,
  kFeatCrossWords,  // perpendicular words formed
  kFeatHooks,       // of those, the ones the new tile ends (front or back hooks)
  kFeatThrough,     // board tiles in the main word
  kFeatCenterDist,  // mean Manhattan distance of the new tiles to the centre
  kFeatVertical,
  // E. Defence.
  kFeatOpensTW,     // empty triple-word squares newly in reach of a tile
  kFeatBingoLanes,  // change in lines with a playable run of seven empty squares
  // F. Ranking within the turn.
  kFeatScoreRank,    // log(1 + rank by score)
  kFeatEquityZ,      // equity standardized over the turn's options
  kFeatSecondGap,    // best minus second best equity (turn level)
  kFeatNonBingoGap,  // best non-bingo equity minus this option's
  // G. Context (turn level).
  kFeatScoreDiff,  // mover's score minus the opponent's, / 100
  kFeatBagTiles,
  kFeatTurnNumber,
  kFeatEndgame,       // empty bag
  kFeatPrevExchange,  // the mover exchanged on their previous turn
  kFeatRating,        // (rating - 1500) / 500, 0 if unknown
  kFeatRatingKnown,
  // H. Interactions (phase = share of the bag drawn, lead = tanh(diff / 100)).
  kFeatGapPhase,
  kFeatScorePhase,
  kFeatLeavePhase,
  kFeatBingoPhase,
  kFeatExchangePhase,
  kFeatGapRating,
  kFeatLogRankRating,
  kFeatMissedBingoRating,
  kFeatZipfRating,
  kFeatGapLead,
  kFeatScoreLead,
  kFeatExchangeLead,
  kNumMoveFeatures,
};
inline constexpr int kNumBaseFeatures = kFeatPass + 1;

const std::array<const char*, kNumMoveFeatures>& MoveFeatureNames();
// Index of a feature by name, -1 if there is none.
int MoveFeatureIndex(std::string_view name);

struct Option {
  Move move;
  double equity = 0;
  std::array<float, kNumMoveFeatures> features{};
};

// Every legal option for `rack` (placements, exchanges when the bag holds at
// least seven tiles, and the pass), best static equity first, with features.
// `unseen` is what the mover cannot see (for the empty-bag equity).
std::vector<Option> RankOptions(MoveGenerator& gen, const Board& board, const Rack& rack,
                                int bag_size, const Rack& unseen, const Leaves& leaves);

// What the mover knows besides the board and their rack.
struct TurnContext {
  int bag_size = 0;
  int score_diff = 0;  // mover minus opponent
  int turn = 0;        // placements, exchanges and passes before this one
  bool prev_exchange = false;
  int rating = 0;  // 0 if unknown
};

// Word data for the A and B features: the playable words (anagrams, bingo
// draws), English frequency and play counts, and a second word list.
class FeatureLexicon {
 public:
  struct WordInfo {
    float zipf = 0;   // wordfreq Zipf frequency, 0 if absent
    float plays = 0;  // times played in the train games
    bool alt = false;
  };

  // `words`: one word per line (first token). `freq`: TSV lines
  // WORD<TAB>zipf<TAB>plays, a header line allowed. `alt`: a second word list.
  // Empty paths are skipped; throws std::runtime_error on a missing file.
  static FeatureLexicon Load(const std::string& words, const std::string& freq = "",
                             const std::string& alt = "");
  static FeatureLexicon FromWords(const std::vector<std::string>& words);
  void SetInfo(const std::string& word, WordInfo info) { info_[word] = info; }
  void MarkAlt(const std::string& word) { info_[word].alt = true; }

  const WordInfo* Find(const std::string& word) const;
  // Seven-letter words using exactly these tiles ('?' stands for any letter).
  int SevenAnagrams(const Rack& rack) const;
  bool HasSeven(const Rack& rack) const { return SevenAnagrams(rack) > 0; }
  std::size_t NumSevens() const { return sevens_.size(); }
  // Exact chance that `leave` plus letters drawn to seven tiles (from a full
  // bag without the leave; a drawn blank counts as a miss) has a seven-letter
  // anagram. Tabulated when the words are loaded (a few seconds for CSW).
  double LeaveBingoProbability(const Rack& leave) const;

 private:
  void TabulateLeaves();

  std::unordered_map<std::string, WordInfo> info_;
  std::unordered_map<std::uint64_t, int> sevens_;  // sorted letters -> words
  // (leave letters, blanks on it) -> chance.
  std::unordered_map<std::uint64_t, float> leave_bingo_;
};

// The main word of a placement, uppercase, with the board's played-through letters.
std::string MainWord(const Board& board, const Move& move);

// Fills the features beyond the base ones.
class FeatureComputer {
 public:
  // `lexicon` may be null: the word features are then 0.
  FeatureComputer(const Leaves& leaves, const FeatureLexicon* lexicon);

  // `options` as returned by RankOptions for `rack` on `board`; fills options
  // [0, k) and `extra` (when it is a valid index).
  void Fill(const Board& board, const Rack& rack, const TurnContext& ctx,
            std::vector<Option>& options, int k, int extra = -1);
  // FeatureLexicon::LeaveBingoProbability, 0 without a lexicon.
  double LeaveBingoProbability(const Rack& leave) const;

 private:
  const FeatureLexicon* lexicon_;
  std::array<float, kNumTileTypes> single_{};
  std::array<std::array<float, kNumTileTypes>, kNumTileTypes> pair_{};  // synergy
};

// True if both moves put the same tiles on the same squares (one-tile plays
// match in either direction), exchange the same tiles, or both pass.
bool SamePlay(const Move& a, const Move& b);

// Index of `move` in `options`, or -1 if it is not there (e.g. a phony).
int FindOption(const std::vector<Option>& options, const Move& move);

// Tiles in the bag when `board` holds `on_board` tiles and both racks were filled.
int BagSizeFor(int on_board);
int TilesOnBoard(const Board& board);

// One turn of a GCG game with the mover's rack, replayed to the board it was
// played on. `turn` counts placements, exchanges and passes from 0, as the
// Python turn store does.
struct ReplayedTurn {
  int turn = 0;
  int player = 0;
  int score_diff = 0;  // mover's score minus the opponent's before the move
  bool prev_exchange = false;
  Rack rack;
  bool known_rack = false;
  Board board;  // before the move
  int bag_size = 0;
  Move move;  // normalized: '.' for played-through squares
  bool phony = false;
};

// Turns of a replayed game; empty if the replay failed.
std::vector<ReplayedTurn> ReplayTurns(const GcgGame& game, std::string* error = nullptr);

}  // namespace scrabble
