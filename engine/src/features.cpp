#include "scrabble/features.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "scrabble/equity.hpp"
#include "scrabble/game.hpp"

namespace scrabble {

namespace {

// (row, column, tile char) of every tile a placement puts on the board.
std::set<std::tuple<int, int, char>> Placed(const Move& m) {
  std::set<std::tuple<int, int, char>> out;
  const int dr = m.dir == Direction::kVertical ? 1 : 0, dc = 1 - dr;
  for (int k = 0; k < static_cast<int>(m.tiles.size()); ++k)
    if (m.tiles[k] != '.') out.emplace(m.row + dr * k, m.col + dc * k, m.tiles[k]);
  return out;
}

Rack TilesOf(const std::string& tiles) {
  Rack r;
  for (char c : tiles)
    if (auto t = TileFromChar(c)) r.Add(*t);
  return r;
}

}  // namespace

const std::array<const char*, kNumMoveFeatures>& MoveFeatureNames() {
  static const std::array<const char*, kNumMoveFeatures> names = [] {
    std::array<const char*, kNumMoveFeatures> n{};
    const char* fixed[] = {
        "gap", "score", "leave", "log_rank", "tiles", "bingo", "exchange", "pass",
        "word_zipf", "word_plays", "word_unfamiliar", "word_length", "word_alt",
        "word_log_prob", "rack_anagrams",
        "had_bingo", "bingo_count", "bingo_easiest", "bingo_gap", "missed_bingo",
        "leave_size", "leave_vowels", "leave_consonants", "leave_duplicates", "leave_s",
        "leave_blank", "leave_q_no_u", "leave_synergy", "leave_bingo_prob"};
    int i = 0;
    for (const char* f : fixed) n[i++] = f;
    static const std::array<std::string, kNumTileTypes> counts = [] {
      std::array<std::string, kNumTileTypes> c;
      for (int t = 0; t < kNumTileTypes; ++t)
        c[t] = t == kBlank ? "leave_n_blank" : std::string("leave_n_") + char('a' + t - 1);
      return c;
    }();
    for (const std::string& c : counts) n[i++] = c.c_str();
    const char* rest[] = {
        "on_dl", "on_tl", "on_dw", "on_tw", "cross_words", "hooks", "through", "center_dist",
        "vertical", "opens_tw", "bingo_lanes", "score_rank", "equity_z", "second_gap",
        "non_bingo_gap", "score_diff", "bag_tiles", "turn_number", "endgame", "prev_exchange",
        "rating", "rating_known", "gap_x_phase", "score_x_phase", "leave_x_phase",
        "bingo_x_phase", "exchange_x_phase", "gap_x_rating", "log_rank_x_rating",
        "missed_bingo_x_rating", "zipf_x_rating", "gap_x_lead", "score_x_lead",
        "exchange_x_lead"};
    for (const char* f : rest) n[i++] = f;
    if (i != kNumMoveFeatures) throw std::logic_error("MoveFeatureNames out of date");
    return n;
  }();
  return names;
}

int MoveFeatureIndex(std::string_view name) {
  const auto& names = MoveFeatureNames();
  for (int i = 0; i < kNumMoveFeatures; ++i)
    if (name == names[i]) return i;
  return -1;
}

std::vector<Option> RankOptions(MoveGenerator& gen, const Board& board, const Rack& rack,
                                int bag_size, const Rack& unseen, const Leaves& leaves) {
  std::vector<Move> moves = gen.Generate(board, rack);
  if (bag_size >= Game::kMinBagForExchange) {
    std::vector<Move> exchanges = GenerateExchanges(rack);
    moves.insert(moves.end(), exchanges.begin(), exchanges.end());
  }
  moves.emplace_back();  // pass
  std::vector<Option> options;
  options.reserve(moves.size());
  for (Move& m : moves) {
    Option o;
    o.equity = StaticEquity(m, rack, bag_size, unseen, leaves);
    o.move = std::move(m);
    options.push_back(std::move(o));
  }
  std::sort(options.begin(), options.end(), [](const Option& a, const Option& b) {
    return a.equity != b.equity ? a.equity > b.equity : TiebreaksBetter(a.move, b.move);
  });
  const double best = options.front().equity;
  for (std::size_t i = 0; i < options.size(); ++i) {
    Option& o = options[i];
    const Move& m = o.move;
    const int tiles =
        m.kind == Move::Kind::kExchange ? static_cast<int>(m.tiles.size()) : m.TilesPlayed();
    auto& f = o.features;
    f[kFeatGap] = static_cast<float>(best - o.equity);
    f[kFeatScore] = static_cast<float>(m.score);
    f[kFeatLeave] = bag_size > 0 ? static_cast<float>(leaves.Value(LeaveAfter(rack, m))) : 0.f;
    f[kFeatLogRank] = static_cast<float>(std::log1p(static_cast<double>(i)));
    f[kFeatTiles] = static_cast<float>(tiles);
    f[kFeatBingo] = m.kind == Move::Kind::kPlace && tiles == kRackSize ? 1.f : 0.f;
    f[kFeatExchange] = m.kind == Move::Kind::kExchange ? 1.f : 0.f;
    f[kFeatPass] = m.kind == Move::Kind::kPass ? 1.f : 0.f;
  }
  return options;
}

namespace {

// Sorted letters (1..26) packed 5 bits each; at most 12 letters.
std::uint64_t LetterKey(std::array<int, 27>& counts) {
  std::uint64_t key = 0;
  for (int t = 1; t < kNumTileTypes; ++t)
    for (int n = 0; n < counts[t]; ++n) key = key << 5 | static_cast<std::uint64_t>(t);
  return key;
}

double LogChoose(int n, int k) {
  if (k < 0 || k > n) return -INFINITY;
  return std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
}

// LogChoose for n <= 100 and k <= 15 (a word's letters and the bag), tabulated.
double LogChooseSmall(int n, int k) {
  static const auto table = [] {
    std::array<std::array<double, 16>, 101> t{};
    for (int a = 0; a <= 100; ++a)
      for (int b = 0; b < 16; ++b) t[a][b] = LogChoose(a, b);
    return t;
  }();
  return n <= 100 && k < 16 ? table[n][k] : LogChoose(n, k);
}

bool IsVowel(Tile t) { return t == 1 || t == 5 || t == 9 || t == 15 || t == 21; }  // AEIOU

constexpr Tile kQ = 17, kU = 21, kS = 19;
constexpr int kMask = (1 << kBoardDim) - 1;

// Occupied squares, one bit per column (rows) or row (cols).
struct Occupancy {
  std::array<std::uint16_t, kBoardDim> rows{}, cols{};
  void Set(int r, int c) {
    rows[r] |= static_cast<std::uint16_t>(1u << c);
    cols[c] |= static_cast<std::uint16_t>(1u << r);
  }
  bool Has(int r, int c) const { return rows[r] >> c & 1; }
};

Occupancy OccupancyOf(const Board& board) {
  Occupancy o;
  for (int r = 0; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c)
      if (!board.IsEmpty(r, c)) o.Set(r, c);
  return o;
}

constexpr std::array<std::pair<int, int>, 8> kTripleWords = {
    {{0, 0}, {0, 7}, {0, 14}, {7, 0}, {7, 14}, {14, 0}, {14, 7}, {14, 14}}};

// Empty triple-word squares with a tile within seven squares in their row or
// column, one bit per kTripleWords entry.
int TripleWordsInReach(const Occupancy& o) {
  int reach = 0;
  for (int i = 0; i < static_cast<int>(kTripleWords.size()); ++i) {
    const auto [r, c] = kTripleWords[i];
    if (o.Has(r, c)) continue;
    const int lo_c = std::max(0, c - 7), hi_c = std::min(kBoardDim - 1, c + 7);
    const int lo_r = std::max(0, r - 7), hi_r = std::min(kBoardDim - 1, r + 7);
    const int row_window = ((1 << (hi_c + 1)) - 1) & ~((1 << lo_c) - 1);
    const int col_window = ((1 << (hi_r + 1)) - 1) & ~((1 << lo_r) - 1);
    if ((o.rows[r] & row_window) || (o.cols[c] & col_window)) reach |= 1 << i;
  }
  return reach;
}

// Of the empty squares out of reach (`unreached` bits), those a new tile brings in reach.
int TripleWordsOpened(int unreached, const std::array<std::pair<int, int>, kRackSize>& placed,
                      int n) {
  int opened = 0;
  for (; unreached; unreached &= unreached - 1) {
    const auto [r, c] = kTripleWords[std::countr_zero(static_cast<unsigned>(unreached))];
    bool hit = false, covered = false;
    for (int j = 0; j < n; ++j) {
      const auto [pr, pc] = placed[j];
      covered |= pr == r && pc == c;
      hit |= (pr == r && std::abs(pc - c) <= 7) || (pc == c && std::abs(pr - r) <= 7);
    }
    opened += hit && !covered;
  }
  return opened;
}

// Lines whose free squares hold a run of seven touching a tile (a bingo lane).
int BingoLanes(const Occupancy& o) {
  int lanes = 0;
  auto count = [&](const std::array<std::uint16_t, kBoardDim>& lines) {
    for (int i = 0; i < kBoardDim; ++i) {
      const int occ = lines[i], empty = ~occ & kMask;
      int adjacent = (occ << 1 | occ >> 1) & kMask;
      if (i > 0) adjacent |= lines[i - 1];
      if (i + 1 < kBoardDim) adjacent |= lines[i + 1];
      adjacent &= empty;
      if (!adjacent) continue;
      int starts = empty;
      for (int s = 1; s < kRackSize; ++s) starts &= empty >> s;
      for (; starts; starts &= starts - 1) {
        const int s = std::countr_zero(static_cast<unsigned>(starts));
        if (adjacent & (0x7F << s)) {
          ++lanes;
          break;
        }
      }
    }
  };
  count(o.rows);
  count(o.cols);
  return lanes;
}

}  // namespace

FeatureLexicon FeatureLexicon::FromWords(const std::vector<std::string>& words) {
  FeatureLexicon lex;
  for (const std::string& w : words) {
    if (w.size() != kRackSize) continue;
    std::array<int, 27> counts{};
    bool ok = true;
    for (char ch : w) {
      const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      if (u < 'A' || u > 'Z') ok = false;
      else ++counts[u - 'A' + 1];
    }
    if (ok) ++lex.sevens_[LetterKey(counts)];
  }
  lex.TabulateLeaves();
  return lex;
}

void FeatureLexicon::TabulateLeaves() {
  const auto& dist = EnglishTiles();
  const int total = TotalTiles();
  // With b blanks on the leave, the letters on the rack after the draw must be
  // 7 - b letters of some word: those letter sets, from the words' subsets.
  std::array<std::unordered_map<std::uint64_t, std::array<int, 27>>, 3> sets;
  for (const auto& entry : sevens_) {
    std::array<int, 27> word{};
    for (int i = 0; i < kRackSize; ++i) ++word[(entry.first >> (5 * i)) & 31];
    sets[0].emplace(entry.first, word);
    for (int x = 1; x < kNumTileTypes; ++x) {
      if (!word[x]) continue;
      --word[x];
      sets[1].emplace(LetterKey(word), word);
      for (int y = x; y < kNumTileTypes; ++y) {
        if (!word[y]) continue;
        --word[y];
        sets[2].emplace(LetterKey(word), word);
        ++word[y];
      }
      ++word[x];
    }
  }
  // choose[n][k] for the pools and draws of a rack.
  std::array<std::array<double, kRackSize + 1>, 128> choose{};
  for (int n = 0; n < 128; ++n) {
    choose[n][0] = 1;
    for (int k = 1; k <= kRackSize; ++k)
      choose[n][k] = k > n ? 0 : choose[n][k - 1] * (n - k + 1) / k;
  }
  std::size_t entries = 0;
  for (const auto& level : sets) entries += level.size() * 16;
  leave_bingo_.reserve(entries);
  for (int blanks = 0; blanks < 3; ++blanks) {
    for (const auto& [key, target] : sets[blanks]) {
      // Distinct letters of the target, ascending, and how many of each the leave keeps.
      int letters[kRackSize], need[kRackSize], keep[kRackSize] = {}, m = 0;
      for (int t = 1; t < kNumTileTypes; ++t)
        if (target[t]) letters[m] = t, need[m++] = target[t];
      // Every leave inside the target: the draw must be exactly the rest.
      while (true) {
        int size = 0;
        double ways = 1;
        std::uint64_t leave_key = 0;
        for (int j = 0; j < m; ++j) {
          size += keep[j];
          ways *= choose[std::max(0, dist[letters[j]].count - keep[j])][need[j] - keep[j]];
          for (int n = 0; n < keep[j]; ++n) leave_key = leave_key << 5 | letters[j];
        }
        if (ways > 0) {
          const int pool = total - size - blanks, draw = kRackSize - blanks - size;
          leave_bingo_[leave_key << 2 | blanks] += static_cast<float>(ways / choose[pool][draw]);
        }
        int j = 0;
        while (j < m && keep[j] == need[j]) keep[j++] = 0;
        if (j == m) break;
        ++keep[j];
      }
    }
  }
}

double FeatureLexicon::LeaveBingoProbability(const Rack& leave) const {
  std::array<int, 27> counts{};
  for (int t = 1; t < kNumTileTypes; ++t) counts[t] = leave.Count(Tile(t));
  const int blanks = leave.Count(kBlank);
  if (blanks > 2) return 0;
  auto it = leave_bingo_.find(LetterKey(counts) << 2 | static_cast<std::uint64_t>(blanks));
  return it == leave_bingo_.end() ? 0.0 : std::min(1.0, static_cast<double>(it->second));
}

namespace {

std::vector<std::string> ReadWords(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open word list: " + path);
  std::vector<std::string> words;
  for (std::string line; std::getline(in, line);) {
    line.resize(std::min(line.size(), line.find_first_of(" \t\r")));
    for (char& c : line) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (!line.empty()) words.push_back(line);
  }
  std::sort(words.begin(), words.end());
  words.erase(std::unique(words.begin(), words.end()), words.end());
  return words;
}

}  // namespace

FeatureLexicon FeatureLexicon::Load(const std::string& words, const std::string& freq,
                                    const std::string& alt) {
  FeatureLexicon lex = FromWords(ReadWords(words));
  if (!freq.empty()) {
    std::ifstream in(freq);
    if (!in) throw std::runtime_error("cannot open word frequencies: " + freq);
    for (std::string line; std::getline(in, line);) {
      std::istringstream ss(line);
      std::string word;
      WordInfo info;
      if (!(ss >> word >> info.zipf >> info.plays)) continue;  // the header, blank lines
      for (char& c : word) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      info.alt = lex.info_[word].alt;
      lex.info_[word] = info;
    }
  }
  if (!alt.empty())
    for (const std::string& w : ReadWords(alt)) lex.info_[w].alt = true;
  return lex;
}

const FeatureLexicon::WordInfo* FeatureLexicon::Find(const std::string& word) const {
  auto it = info_.find(word);
  return it == info_.end() ? nullptr : &it->second;
}

int FeatureLexicon::SevenAnagrams(const Rack& rack) const {
  if (rack.Size() != kRackSize || sevens_.empty()) return 0;
  std::array<int, 27> counts{};
  for (int t = 1; t < kNumTileTypes; ++t) counts[t] = rack.Count(Tile(t));
  // Blank designations in ascending order, so each multiset is counted once.
  int total = 0;
  auto rec = [&](auto& self, int blanks, int from) -> void {
    if (blanks == 0) {
      if (auto it = sevens_.find(LetterKey(counts)); it != sevens_.end()) total += it->second;
      return;
    }
    for (int t = from; t < kNumTileTypes; ++t) {
      ++counts[t];
      self(self, blanks - 1, t);
      --counts[t];
    }
  };
  rec(rec, rack.Count(kBlank), 1);
  return total;
}

std::string MainWord(const Board& board, const Move& move) {
  std::string word;
  if (move.kind != Move::Kind::kPlace) return word;
  const int dr = move.dir == Direction::kVertical ? 1 : 0, dc = 1 - dr;
  for (int k = 0; k < static_cast<int>(move.tiles.size()); ++k) {
    char c = move.tiles[k];
    if (c == '.') {
      const int r = move.row + dr * k, col = move.col + dc * k;
      c = TileToChar(static_cast<Tile>(board.At(r, col) & ~Board::kBlankBit));
    }
    word += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return word;
}

FeatureComputer::FeatureComputer(const Leaves& leaves, const FeatureLexicon* lexicon)
    : leaves_(leaves), lexicon_(lexicon) {
  for (int a = 0; a < kNumTileTypes; ++a) {
    Rack r;
    r.Add(Tile(a));
    single_[a] = static_cast<float>(leaves.Value(r));
  }
  for (int a = 0; a < kNumTileTypes; ++a)
    for (int b = a; b < kNumTileTypes; ++b) {
      Rack r;
      r.Add(Tile(a));
      r.Add(Tile(b));
      pair_[a][b] = pair_[b][a] = static_cast<float>(leaves.Value(r)) - single_[a] - single_[b];
    }
}

double FeatureComputer::LeaveBingoProbability(const Rack& leave) const {
  return lexicon_ && leave.Size() <= kRackSize ? lexicon_->LeaveBingoProbability(leave) : 0.0;
}

void FeatureComputer::Fill(const Board& board, const Rack& rack, const TurnContext& ctx,
                           std::vector<Option>& options, int k, int extra) {
  const int n = static_cast<int>(options.size());
  if (n == 0) return;
  k = std::min(k, n);
  // Turn level: bingos, score order, equity spread, board reach.
  int bingos = 0;
  double best_bingo = 0, best_non_bingo = 0, easiest = 0;
  bool have_non_bingo = false;
  double sum = 0, sum_sq = 0;
  std::vector<int> scores(n);
  for (int i = 0; i < n; ++i) {
    const Option& o = options[i];
    scores[i] = o.move.score;
    sum += o.equity;
    sum_sq += o.equity * o.equity;
    if (o.features[kFeatBingo] > 0) {
      if (bingos++ == 0) best_bingo = o.equity;
      if (lexicon_)
        if (const auto* info = lexicon_->Find(MainWord(board, o.move)))
          easiest = std::max(easiest, static_cast<double>(info->zipf));
    } else if (!have_non_bingo) {
      have_non_bingo = true;
      best_non_bingo = o.equity;
    }
  }
  std::sort(scores.begin(), scores.end(), std::greater<>());
  const double mean = sum / n, sd = std::sqrt(std::max(0.0, sum_sq / n - mean * mean));
  const double best = options.front().equity;
  const double second_gap = n > 1 ? best - options[1].equity : 0;
  const int anagrams = lexicon_ ? lexicon_->SevenAnagrams(rack) : 0;
  const Occupancy before = OccupancyOf(board);
  const int empty_tw = [&] {
    int bits = 0;
    for (int i = 0; i < static_cast<int>(kTripleWords.size()); ++i)
      if (!before.Has(kTripleWords[i].first, kTripleWords[i].second)) bits |= 1 << i;
    return bits;
  }();
  const int tw_unreached = empty_tw & ~TripleWordsInReach(before);
  const int lanes_before = BingoLanes(before);
  const double drawn_share = 1.0 - static_cast<double>(ctx.bag_size) / BagSizeFor(0);
  const double phase = std::clamp(drawn_share, 0.0, 1.0);
  const double lead = std::tanh(ctx.score_diff / 100.0);
  const double rating = ctx.rating > 0 ? (ctx.rating - 1500) / 500.0 : 0.0;
  const auto& dist = EnglishTiles();

  auto fill = [&](Option& o) {
    auto& f = o.features;
    const Move& m = o.move;
    // A.
    if (m.kind == Move::Kind::kPlace) {
      const std::string word = MainWord(board, m);
      const FeatureLexicon::WordInfo* info = lexicon_ ? lexicon_->Find(word) : nullptr;
      f[kFeatWordZipf] = info ? info->zipf : 0.f;
      f[kFeatWordPlays] = info ? static_cast<float>(std::log1p(info->plays)) : 0.f;
      f[kFeatWordUnfamiliar] = !info || (info->zipf <= 0 && info->plays <= 0) ? 1.f : 0.f;
      f[kFeatWordAlt] = info && info->alt ? 1.f : 0.f;
      f[kFeatWordLength] = static_cast<float>(word.size());
      std::array<int, 27> counts{};
      for (char c : word) ++counts[c - 'A' + 1];
      const int len = static_cast<int>(word.size());
      double lp = -LogChooseSmall(TotalTiles(), len);
      for (int t = 1; t < kNumTileTypes; ++t) {
        if (!counts[t]) continue;
        const int have = std::min(counts[t], static_cast<int>(dist[t].count));
        lp += LogChooseSmall(dist[t].count, have) + (counts[t] - have) * std::log(1e-3);
      }
      f[kFeatWordLogProb] = static_cast<float>(lp / std::max(1, len));
    }
    f[kFeatRackAnagrams] = static_cast<float>(std::log1p(anagrams));
    // B.
    f[kFeatHadBingo] = bingos > 0 ? 1.f : 0.f;
    f[kFeatBingoCount] = static_cast<float>(std::log1p(bingos));
    f[kFeatBingoEasiest] = static_cast<float>(easiest);
    f[kFeatBingoGap] = bingos > 0 ? static_cast<float>(best_bingo - o.equity) : 0.f;
    f[kFeatMissedBingo] = bingos > 0 && f[kFeatBingo] == 0 ? 1.f : 0.f;
    // C.
    const Rack leave = LeaveAfter(rack, m);
    int vowels = 0, dups = 0;
    float synergy = 0;
    std::array<Tile, kRackSize> tiles{};
    int held = 0;
    for (int t = 0; t < kNumTileTypes; ++t) {
      const int c = leave.Count(Tile(t));
      f[kFeatLeaveCount0 + t] = static_cast<float>(c);
      if (IsVowel(Tile(t))) vowels += c;
      dups += std::max(0, c - 1);
      for (int j = 0; j < c && held < kRackSize; ++j) tiles[held++] = Tile(t);
    }
    for (int a = 0; a < held; ++a)
      for (int b = a + 1; b < held; ++b) synergy += pair_[tiles[a]][tiles[b]];
    f[kFeatLeaveSize] = static_cast<float>(leave.Size());
    f[kFeatLeaveVowels] = static_cast<float>(vowels);
    f[kFeatLeaveConsonants] = static_cast<float>(leave.Size() - vowels - leave.Count(kBlank));
    f[kFeatLeaveDuplicates] = static_cast<float>(dups);
    f[kFeatLeaveS] = static_cast<float>(leave.Count(kS));
    f[kFeatLeaveBlank] = static_cast<float>(leave.Count(kBlank));
    f[kFeatLeaveQNoU] = leave.Count(kQ) > 0 && leave.Count(kU) == 0 ? 1.f : 0.f;
    f[kFeatLeaveSynergy] = synergy;
    f[kFeatLeaveBingoProb] =
        ctx.bag_size > 0 ? static_cast<float>(LeaveBingoProbability(leave)) : 0.f;
    // D and E.
    if (m.kind == Move::Kind::kPlace) {
      const int dr = m.dir == Direction::kVertical ? 1 : 0, dc = 1 - dr;
      int on[5] = {0, 0, 0, 0, 0}, cross = 0, hooks = 0, through = 0, placed = 0, dist_sum = 0;
      std::array<std::pair<int, int>, kRackSize> squares{};
      Occupancy after = before;
      for (int i = 0; i < static_cast<int>(m.tiles.size()); ++i) {
        if (m.tiles[i] == '.') {
          ++through;
          continue;
        }
        const int r = m.row + dr * i, c = m.col + dc * i;
        if (placed < kRackSize) squares[placed] = {r, c};
        ++placed;
        ++on[static_cast<int>(Board::BonusAt(r, c))];
        dist_sum += std::abs(r - kCenter) + std::abs(c - kCenter);
        after.Set(r, c);
        // Perpendicular neighbours are board tiles only.
        const int pr = dc, pc = dr;
        const bool lo = r - pr >= 0 && c - pc >= 0 && !board.IsEmpty(r - pr, c - pc);
        const bool hi = r + pr < kBoardDim && c + pc < kBoardDim && !board.IsEmpty(r + pr, c + pc);
        cross += lo || hi;
        hooks += lo != hi;
      }
      f[kFeatOnDL] = static_cast<float>(on[static_cast<int>(Bonus::kDoubleLetter)]);
      f[kFeatOnTL] = static_cast<float>(on[static_cast<int>(Bonus::kTripleLetter)]);
      f[kFeatOnDW] = static_cast<float>(on[static_cast<int>(Bonus::kDoubleWord)]);
      f[kFeatOnTW] = static_cast<float>(on[static_cast<int>(Bonus::kTripleWord)]);
      f[kFeatCrossWords] = static_cast<float>(cross);
      f[kFeatHooks] = static_cast<float>(hooks);
      f[kFeatThrough] = static_cast<float>(through);
      f[kFeatCenterDist] = placed ? static_cast<float>(dist_sum) / placed : 0.f;
      f[kFeatVertical] = m.dir == Direction::kVertical ? 1.f : 0.f;
      f[kFeatOpensTW] =
          static_cast<float>(TripleWordsOpened(tw_unreached, squares, std::min(placed, kRackSize)));
      f[kFeatBingoLanes] = static_cast<float>(BingoLanes(after) - lanes_before);
    }
    // F.
    // Options scoring strictly more.
    const auto score_rank =
        std::lower_bound(scores.begin(), scores.end(), m.score, std::greater<>()) - scores.begin();
    f[kFeatScoreRank] = static_cast<float>(std::log1p(static_cast<double>(score_rank)));
    f[kFeatEquityZ] = sd > 0 ? static_cast<float>((o.equity - mean) / sd) : 0.f;
    f[kFeatSecondGap] = static_cast<float>(second_gap);
    f[kFeatNonBingoGap] = have_non_bingo ? static_cast<float>(best_non_bingo - o.equity) : 0.f;
    // G.
    f[kFeatScoreDiff] = static_cast<float>(ctx.score_diff / 100.0);
    f[kFeatBagTiles] = static_cast<float>(ctx.bag_size);
    f[kFeatTurnNumber] = static_cast<float>(ctx.turn);
    f[kFeatEndgame] = ctx.bag_size == 0 ? 1.f : 0.f;
    f[kFeatPrevExchange] = ctx.prev_exchange ? 1.f : 0.f;
    f[kFeatRating] = static_cast<float>(rating);
    f[kFeatRatingKnown] = ctx.rating > 0 ? 1.f : 0.f;
    // H.
    f[kFeatGapPhase] = static_cast<float>(f[kFeatGap] * phase);
    f[kFeatScorePhase] = static_cast<float>(f[kFeatScore] * phase);
    f[kFeatLeavePhase] = static_cast<float>(f[kFeatLeave] * phase);
    f[kFeatBingoPhase] = static_cast<float>(f[kFeatBingo] * phase);
    f[kFeatExchangePhase] = static_cast<float>(f[kFeatExchange] * phase);
    f[kFeatGapRating] = static_cast<float>(f[kFeatGap] * rating);
    f[kFeatLogRankRating] = static_cast<float>(f[kFeatLogRank] * rating);
    f[kFeatMissedBingoRating] = static_cast<float>(f[kFeatMissedBingo] * rating);
    f[kFeatZipfRating] = static_cast<float>(f[kFeatWordZipf] * rating);
    f[kFeatGapLead] = static_cast<float>(f[kFeatGap] * lead);
    f[kFeatScoreLead] = static_cast<float>(f[kFeatScore] * lead);
    f[kFeatExchangeLead] = static_cast<float>(f[kFeatExchange] * lead);
  };
  for (int i = 0; i < k; ++i) fill(options[i]);
  if (extra >= k && extra < n) fill(options[extra]);
}

bool SamePlay(const Move& a, const Move& b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
    case Move::Kind::kPass:
      return true;
    case Move::Kind::kExchange:
      return TilesOf(a.tiles) == TilesOf(b.tiles);
    case Move::Kind::kPlace:
      break;
  }
  return Placed(a) == Placed(b);
}

int FindOption(const std::vector<Option>& options, const Move& move) {
  for (std::size_t i = 0; i < options.size(); ++i)
    if (SamePlay(options[i].move, move)) return static_cast<int>(i);
  return -1;
}

int TilesOnBoard(const Board& board) {
  int n = 0;
  for (int r = 0; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c) n += !board.IsEmpty(r, c);
  return n;
}

int BagSizeFor(int on_board) { return std::max(0, TotalTiles() - 2 * kRackSize - on_board); }

std::vector<ReplayedTurn> ReplayTurns(const GcgGame& game, std::string* error) {
  GcgReplay replay = ReplayGcg(game);
  if (!replay.ok) {
    if (error) *error = replay.error;
    return {};
  }
  std::vector<ReplayedTurn> turns;
  std::array<int, 2> scores{};
  std::array<bool, 2> exchanged{};
  for (std::size_t i = 0; i < game.events.size(); ++i) {
    const GcgEvent& ev = game.events[i];
    const std::array<int, 2> before = scores;
    if (ev.player == 0 || ev.player == 1) scores[ev.player] = ev.cumulative;
    if (ev.kind == GcgEvent::Kind::kPhonyWithdrawn && !turns.empty()) turns.back().phony = true;
    if (ev.kind != GcgEvent::Kind::kPlace && ev.kind != GcgEvent::Kind::kExchange &&
        ev.kind != GcgEvent::Kind::kPass)
      continue;
    ReplayedTurn t;
    t.turn = static_cast<int>(turns.size());
    t.player = ev.player;
    t.score_diff = before[ev.player & 1] - before[1 - (ev.player & 1)];
    t.prev_exchange = exchanged[ev.player & 1];
    exchanged[ev.player & 1] = ev.kind == GcgEvent::Kind::kExchange;
    if (!ev.rack.empty()) {
      if (auto r = Rack::FromString(ev.rack)) {
        t.rack = *r;
        t.known_rack = true;
      }
    }
    t.board = replay.boards_before[i];
    t.bag_size = BagSizeFor(TilesOnBoard(t.board));
    t.move = replay.moves[i];
    // Exchanges of unknown tiles carry no tiles: they cannot be matched.
    if (t.move.kind == Move::Kind::kExchange && t.move.tiles.empty()) t.known_rack = false;
    turns.push_back(std::move(t));
  }
  return turns;
}

}  // namespace scrabble
