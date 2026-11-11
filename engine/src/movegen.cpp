#include "scrabble/movegen.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace scrabble {

namespace {

constexpr std::uint32_t kAllLetters = 0x7FFFFFE;  // bits 1..26

std::uint8_t CellAt(const Board& b, Direction dir, int line, int i) {
  return dir == Direction::kHorizontal ? b.At(line, i) : b.At(i, line);
}

std::uint8_t LetterOf(std::uint8_t cell) { return cell & ~Board::kBlankBit; }

// Face values by tile, as EnglishTiles.
constexpr int kScores[32] = {0, 1, 3, 3,  2, 1, 4, 2, 4, 1, 8, 5, 1, 3,
                             1, 1, 3, 10, 1, 1, 1, 1, 4, 4, 8, 4, 10};

int CellValue(std::uint8_t cell) { return (cell & Board::kBlankBit) ? 0 : kScores[LetterOf(cell)]; }

int LetterMultiplier(Bonus b) {
  return b == Bonus::kDoubleLetter ? 2 : b == Bonus::kTripleLetter ? 3 : 1;
}

int WordMultiplier(Bonus b) {
  return b == Bonus::kDoubleWord ? 2 : b == Bonus::kTripleWord ? 3 : 1;
}

// Sorts a few values high first (std::sort trips -Warray-bounds on small arrays).
void SortDescending(int* v, int n) {
  for (int i = 1; i < n; ++i)
    for (int j = i; j > 0 && v[j] > v[j - 1]; --j) std::swap(v[j], v[j - 1]);
}

void ToBoard(Direction dir, int line, int i, int& row, int& col) {
  row = dir == Direction::kHorizontal ? line : i;
  col = dir == Direction::kHorizontal ? i : line;
}

// Sorted letters, 5 bits each.
std::uint64_t AnagramKey(const std::uint8_t* letters, int n) {
  std::uint8_t sorted[8];
  std::copy(letters, letters + n, sorted);
  std::sort(sorted, sorted + n);
  std::uint64_t key = 0;
  for (int i = 0; i < n; ++i) key = key << 5 | sorted[i];
  return key;
}

// The 7-letter words, and the 7-letter subsets of 8-letter words, by anagram
// key: whether a rack of 7 (no blank) has a bingo through no board tile or one.
struct BingoIndex {
  std::unordered_set<std::uint64_t> seven, eight_less_one;
};

void CollectBingos(const Gaddag& g, Gaddag::Node node, std::uint8_t* word, int depth,
                   BingoIndex& index) {
  if (depth >= 7 && g.IsTerminal(node)) {
    if (depth == 7) index.seven.insert(AnagramKey(word, 7));
    if (depth == 8) {
      for (int skip = 0; skip < 8; ++skip) {
        std::uint8_t rest[7];
        for (int i = 0, j = 0; i < 8; ++i)
          if (i != skip) rest[j++] = word[i];
        index.eight_less_one.insert(AnagramKey(rest, 7));
      }
    }
  }
  if (depth == 8) return;
  // The separator-free path of a word is the word reversed.
  for (std::uint32_t bits = g.LetterMask(node) & kAllLetters; bits; bits &= bits - 1) {
    const auto L = static_cast<std::uint8_t>(std::countr_zero(bits));
    word[depth] = L;
    CollectBingos(g, g.Next(node, L), word, depth + 1, index);
  }
}

const BingoIndex& BingosOf(const Gaddag& g) {
  static std::mutex mutex;
  // By address and size, so a lexicon built later at a freed address is not
  // mistaken for an earlier one.
  struct Id {
    const Gaddag* g;
    std::size_t nodes, edges;
    bool operator==(const Id&) const = default;
  };
  struct IdHash {
    std::size_t operator()(const Id& id) const {
      return std::hash<const void*>()(id.g) ^ id.nodes * 31 ^ id.edges * 131;
    }
  };
  static std::unordered_map<Id, std::unique_ptr<BingoIndex>, IdHash> cache;
  std::lock_guard<std::mutex> lock(mutex);
  std::unique_ptr<BingoIndex>& slot = cache[Id{&g, g.NumNodes(), g.NumEdges()}];
  if (!slot) {
    slot = std::make_unique<BingoIndex>();
    std::uint8_t word[8];
    CollectBingos(g, g.Root(), word, 0, *slot);
  }
  return *slot;
}

}  // namespace

int ScoreMove(const Board& board, const Move& move) {
  const int dr = move.dir == Direction::kVertical ? 1 : 0;
  const int dc = move.dir == Direction::kHorizontal ? 1 : 0;
  int main = 0, word_mult = 1, cross_total = 0;
  for (std::size_t k = 0; k < move.tiles.size(); ++k) {
    int r = move.row + dr * static_cast<int>(k), c = move.col + dc * static_cast<int>(k);
    char ch = move.tiles[k];
    if (ch == '.') {
      main += CellValue(board.At(r, c));
      continue;
    }
    int v =
        std::islower(static_cast<unsigned char>(ch)) ? 0 : EnglishTiles()[*TileFromChar(ch)].score;
    Bonus bonus = Board::BonusAt(r, c);
    int lm = LetterMultiplier(bonus), wm = WordMultiplier(bonus);
    main += v * lm;
    word_mult *= wm;
    // Perpendicular word through this new tile.
    int cross = 0;
    bool has_cross = false;
    for (int s = -1; s <= 1; s += 2) {
      int rr = r + s * dc, cc = c + s * dr;
      while (rr >= 0 && rr < kBoardDim && cc >= 0 && cc < kBoardDim && !board.IsEmpty(rr, cc)) {
        cross += CellValue(board.At(rr, cc));
        has_cross = true;
        rr += s * dc;
        cc += s * dr;
      }
    }
    if (has_cross) cross_total += (cross + v * lm) * wm;
  }
  return main * word_mult + cross_total + (move.TilesPlayed() == kRackSize ? kBingoBonus : 0);
}

void MoveGenerator::set_gaddag(const Gaddag& gaddag) {
  gaddag_ = &gaddag;
  prepared_ = false;
  bingos_ = nullptr;
  for (auto& by_line : cross_cache_)
    for (auto& line : by_line)
      for (CrossEntry& e : line) e.valid = false;
}

std::vector<Move> MoveGenerator::Generate(const Board& board, const Rack& rack) {
  std::vector<Move> moves;
  Generate(board, rack, moves);
  return moves;
}

void MoveGenerator::Generate(const Board& board, const Rack& rack, std::vector<Move>& out) {
  out.clear();
  board_ = &board;
  rack_ = rack;
  rack_letters_ = 0;
  for (int t = 0; t < kNumTileTypes; ++t)
    if (rack.Count(Tile(t)) > 0) rack_letters_ |= 1u << t;
  out_ = &out;
  Prepare(board);
  for (Direction dir : {Direction::kHorizontal, Direction::kVertical}) {
    for (int line = 0; line < kBoardDim; ++line) {
      dir_ = dir;
      line_ = line;
      sq_ = squares_[static_cast<int>(dir)][line];
      if (index_) {
        ForEachRun(rack.Size(), [&](int l, int r, int k) { GenRun(l, r, k); });
      } else {
        GenLine(dir, line);
      }
    }
  }
}

long MoveGenerator::GenerateBest(const Board& board, const Rack& rack, Evaluator eval,
                                 const void* context, Move& best, double& best_value) {
  eval_ = eval;
  eval_context_ = context;
  best_ = &best;
  best_value_ = &best_value;
  found_ = false;
  seen_ = 0;
  board_ = &board;
  rack_ = rack;
  rack_letters_ = 0;
  for (int t = 0; t < kNumTileTypes; ++t)
    if (rack.Count(Tile(t)) > 0) rack_letters_ |= 1u << t;
  out_ = nullptr;
  Prepare(board);

  // Every leave of the rack, valued once: index = sum of count x radix over the
  // rack's distinct tiles. The best value of each size bounds the anchors.
  std::vector<Tile> tiles;
  for (int t = 0; t < kNumTileTypes; ++t)
    for (int k = 0; k < rack.Count(Tile(t)); ++k) tiles.push_back(Tile(t));
  const int n = static_cast<int>(tiles.size());
  Tile distinct[kRackSize];
  int counts[kRackSize], radix[kRackSize], m = 0, combos = 1;
  for (int t = 0; t < kNumTileTypes; ++t) {
    radix_[t] = 0;
    if (rack.Count(Tile(t)) == 0) continue;
    distinct[m] = Tile(t);
    counts[m] = rack.Count(Tile(t));
    radix[m] = combos;
    radix_[t] = combos;
    combos *= counts[m] + 1;
    ++m;
  }
  double best_leave[kRackSize + 1];
  std::fill(best_leave, best_leave + kRackSize + 1, -1e300);
  for (int index = 0; index < combos; ++index) {
    Rack leave;
    for (int j = 0; j < m; ++j)
      for (int k = 0; k < index / radix[j] % (counts[j] + 1); ++k) leave.Add(distinct[j]);
    leave_value_[index] = eval(context, 0, leave);
    const int placed = n - leave.Size();
    best_leave[placed] = std::max(best_leave[placed], leave_value_[index]);
  }
  leave_index_ = combos - 1;  // every tile still on the rack
  int tile_values[kRackSize] = {};
  for (int i = 0; i < n; ++i)
    tile_values[i] = tiles[i] == kBlank ? 0 : EnglishTiles()[tiles[i]].score;
  SortDescending(tile_values, n);

  // A bingo through no board tile, or one, needs a word of the rack (plus a
  // letter); with a blank, assume there is one.
  bingo_[0] = bingo_[1] = true;
  if (n == kRackSize && rack.Count(kBlank) == 0) {
    if (!bingos_) bingos_ = &BingosOf(*gaddag_);
    const auto& index = *static_cast<const BingoIndex*>(bingos_);
    std::uint8_t letters[kRackSize];
    for (int i = 0; i < n; ++i) letters[i] = tiles[i];
    const std::uint64_t key = AnagramKey(letters, n);
    bingo_[0] = index.seven.count(key) > 0;
    bingo_[1] = index.eight_less_one.count(key) > 0;
  }

  for (int i = 0; i < kBoardDim; ++i) placed_[i] = 0;
  if (index_) {
    // Runs by their bound, highest first, until no placement can beat the best.
    struct Run {
      double bound;
      int dir, line, l, r, k;
    };
    std::vector<Run> runs;
    for (int d = 0; d < 2; ++d) {
      for (int line = 0; line < kBoardDim; ++line) {
        dir_ = static_cast<Direction>(d);
        line_ = line;
        sq_ = squares_[d][line];
        ForEachRun(n, [&](int l, int r, int k) {
          runs.push_back({RunBound(l, r, k, tile_values, best_leave), d, line, l, r, k});
        });
      }
    }
    std::stable_sort(runs.begin(), runs.end(),
                     [](const Run& x, const Run& y) { return x.bound > y.bound; });
    for (const Run& run : runs) {
      if (found_ && run.bound < *best_value_) break;
      dir_ = static_cast<Direction>(run.dir);
      line_ = run.line;
      sq_ = squares_[run.dir][run.line];
      GenRun(run.l, run.r, run.k);
    }
    eval_ = nullptr;
    std::fill(radix_, radix_ + kNumTileTypes, 0);
    return seen_;
  }
  // Anchors by their bound, highest first, until no placement can beat the best.
  struct Item {
    double bound;
    int dir, line, anchor;
  };
  std::vector<Item> items;
  for (int d = 0; d < 2; ++d) {
    for (int line = 0; line < kBoardDim; ++line) {
      dir_ = static_cast<Direction>(d);
      line_ = line;
      sq_ = squares_[d][line];
      for (int a = 0; a < kBoardDim; ++a)
        if (sq_[a].anchor)
          items.push_back({AnchorBound(a, tile_values, n, best_leave), d, line, a});
    }
  }
  std::stable_sort(items.begin(), items.end(),
                   [](const Item& x, const Item& y) { return x.bound > y.bound; });
  for (const Item& it : items) {
    if (found_ && it.bound < *best_value_) break;
    dir_ = static_cast<Direction>(it.dir);
    line_ = it.line;
    sq_ = squares_[it.dir][it.line];
    anchor_ = it.anchor;
    tiles_placed_ = 0;
    Gen(0, gaddag_->Root());
  }
  eval_ = nullptr;
  std::fill(radix_, radix_ + kNumTileTypes, 0);
  return seen_;
}

double MoveGenerator::AnchorBound(int a, const int* tile_values, int rack_size,
                                  const double* best_leave) const {
  double best = -1e300;
  auto empty = [&](int i) { return sq_[i].letter == Board::kEmpty; };
  // Empty squares to the left of the anchor, nearest first, and to the right.
  // A placement fills a run of squares, so it stops before an empty square no
  // rack tile can take (its cross set).
  const std::uint32_t playable = (rack_letters_ & 1u) ? kAllLetters : rack_letters_;
  auto usable = [&](int i) { return (sq_[i].cross & playable) != 0; };
  if (!usable(a)) return -1e300;
  int left[kBoardDim], right[kBoardDim], nl = 0, nr = 0;
  for (int i = a - 1; i >= 0; --i)
    if (empty(i)) {
      if (!usable(i)) break;
      left[nl++] = i;
    }
  for (int i = a + 1; i < kBoardDim; ++i)
    if (empty(i)) {
      if (!usable(i)) break;
      right[nr++] = i;
    }
  for (int L = 0; L <= nl && 1 + L <= rack_size; ++L) {
    int lo = L ? left[L - 1] : a;
    while (lo > 0 && !empty(lo - 1)) --lo;  // tiles joined on the left
    for (int R = 0; R <= nr && 1 + L + R <= rack_size; ++R) {
      int hi = R ? right[R - 1] : a;
      while (hi < kBoardDim - 1 && !empty(hi + 1)) ++hi;
      int existing = 0, word_mult = 1, cross_const = 0, k = 0;
      int lm[kBoardDim], wm[kBoardDim];
      bool cross[kBoardDim];
      for (int i = lo; i <= hi; ++i) {
        if (!empty(i)) {
          existing += CellValue(sq_[i].letter);
          continue;
        }
        lm[k] = sq_[i].letter_mult;
        wm[k] = sq_[i].word_mult;
        cross[k] = sq_[i].has_cross;
        word_mult *= wm[k];
        if (cross[k]) cross_const += sq_[i].cross_score * wm[k];
        ++k;
      }
      // Board tiles in the span: with none or one, a bingo needs a word found above.
      if (k == kRackSize && hi - lo + 1 - k <= 1 && !bingo_[hi - lo + 1 - k]) continue;
      int coef[kBoardDim];
      for (int j = 0; j < k; ++j) coef[j] = lm[j] * word_mult + (cross[j] ? lm[j] * wm[j] : 0);
      SortDescending(coef, k);
      int score = existing * word_mult + cross_const + (k == kRackSize ? kBingoBonus : 0);
      for (int j = 0; j < k; ++j) score += tile_values[j] * coef[j];
      best = std::max(best, score + best_leave[k]);
    }
  }
  return best;
}

void MoveGenerator::Prepare(const Board& board) {
  // Square data only changes around tiles that differ from the board of the
  // previous call (consecutive calls in a search differ by a move or two).
  const bool empty = board.IsEmpty(kCenter, kCenter);
  const bool all = !prepared_ || empty != board_empty_;
  board_empty_ = empty;
  constexpr int kH = static_cast<int>(Direction::kHorizontal);
  constexpr int kV = static_cast<int>(Direction::kVertical);
  bool dirty[2][kBoardDim][kBoardDim];
  std::fill(&dirty[0][0][0], &dirty[0][0][0] + 2 * kBoardDim * kBoardDim, all);
  auto occupied = [&](int r, int c) {
    return !board.IsEmpty(r, c) || !prepared_board_.IsEmpty(r, c);
  };
  for (int r = 0; r < kBoardDim && !all; ++r) {
    for (int c = 0; c < kBoardDim; ++c) {
      if (board.At(r, c) == prepared_board_.At(r, c)) continue;
      // Horizontal lines are rows: a square's cross word runs down its column,
      // so the squares joined to (r, c) in its column (on either board) and
      // the empty squares ending that run change, and likewise across its row.
      dirty[kH][r][c] = dirty[kV][c][r] = true;
      for (int d = -1; d <= 1; d += 2) {
        for (int k = r + d; k >= 0 && k < kBoardDim; k += d) {
          dirty[kH][k][c] = dirty[kV][c][k] = true;
          if (!occupied(k, c)) break;
        }
        for (int k = c + d; k >= 0 && k < kBoardDim; k += d) {
          dirty[kH][r][k] = dirty[kV][k][r] = true;
          if (!occupied(r, k)) break;
        }
      }
    }
  }
  for (Direction dir : {Direction::kHorizontal, Direction::kVertical})
    for (int line = 0; line < kBoardDim; ++line)
      for (int i = 0; i < kBoardDim; ++i)
        if (dirty[static_cast<int>(dir)][line][i])
          squares_[static_cast<int>(dir)][line][i] = ComputeSquare(board, dir, line, i);
  prepared_board_ = board;
  prepared_ = true;
}

MoveGenerator::Square MoveGenerator::ComputeSquare(const Board& board, Direction dir, int line,
                                                   int i) {
  const Direction perp =
      dir == Direction::kHorizontal ? Direction::kVertical : Direction::kHorizontal;
  Square s;
  s.letter = CellAt(board, dir, line, i);
  s.cross = kAllLetters;
  s.has_cross = false;
  s.cross_score = 0;
  s.anchor = false;
  {
    int r, c;
    ToBoard(dir, line, i, r, c);
    const Bonus b = Board::BonusAt(r, c);
    s.letter_mult = static_cast<std::uint8_t>(LetterMultiplier(b));
    s.word_mult = static_cast<std::uint8_t>(WordMultiplier(b));
  }
  if (s.letter != Board::kEmpty) return s;

  // Perpendicular neighbours: tiles before (lower index) and after this square.
  int lo = line, hi = line;
  while (lo > 0 && CellAt(board, perp, i, lo - 1) != Board::kEmpty) --lo;
  while (hi < kBoardDim - 1 && CellAt(board, perp, i, hi + 1) != Board::kEmpty) ++hi;
  bool side = (line > 0 && CellAt(board, dir, line - 1, i) != Board::kEmpty) ||
              (line < kBoardDim - 1 && CellAt(board, dir, line + 1, i) != Board::kEmpty);
  bool along = (i > 0 && CellAt(board, dir, line, i - 1) != Board::kEmpty) ||
               (i < kBoardDim - 1 && CellAt(board, dir, line, i + 1) != Board::kEmpty);
  const bool opening_anchor = board_empty_ && line == kCenter && i == kCenter &&
                              (dir == Direction::kHorizontal || vertical_openings_);
  s.anchor = side || along || opening_anchor;
  if (lo == line && hi == line) return s;

  s.has_cross = true;
  for (int k = lo; k <= hi; ++k)
    if (k != line) s.cross_score += CellValue(CellAt(board, perp, i, k));
  // The cross set only depends on the perpendicular word around the square,
  // which rarely changes between calls (endgame search): cache it.
  CrossEntry key;
  key.valid = true;
  key.word[0] = static_cast<std::uint8_t>(line - lo);
  key.word[1] = static_cast<std::uint8_t>(hi - lo);
  for (int k = lo, j = 2; k <= hi; ++k)
    if (k != line) key.word[j++] = LetterOf(CellAt(board, perp, i, k));
  CrossEntry& cached = cross_cache_[static_cast<int>(dir)][line][i];
  if (cached.valid && cached.word == key.word) {
    s.cross = cached.cross;
    return s;
  }
  // Lookup reverse(before + L + after) = reverse(after) + L + reverse(before).
  s.cross = 0;
  Gaddag::Node n = gaddag_->Root();
  for (int k = hi; k > line && n != Gaddag::kNone; --k)
    n = gaddag_->Next(n, LetterOf(CellAt(board, perp, i, k)));
  if (n != Gaddag::kNone) {
    for (std::uint32_t bits = gaddag_->LetterMask(n) & kAllLetters; bits; bits &= bits - 1) {
      const auto L = static_cast<std::uint8_t>(std::countr_zero(bits));
      Gaddag::Node m = gaddag_->Next(n, L);
      for (int k = line - 1; k >= lo && m != Gaddag::kNone; --k)
        m = gaddag_->Next(m, LetterOf(CellAt(board, perp, i, k)));
      if (m != Gaddag::kNone && gaddag_->IsTerminal(m)) s.cross |= 1u << L;
    }
  }
  key.cross = s.cross;
  cached = key;
  return s;
}

void MoveGenerator::GenLine(Direction, int) {
  for (int i = 0; i < kBoardDim; ++i) placed_[i] = 0;
  for (int a = 0; a < kBoardDim; ++a) {
    if (!sq_[a].anchor) continue;
    anchor_ = a;
    tiles_placed_ = 0;
    Gen(0, gaddag_->Root());
  }
}

void MoveGenerator::Gen(int pos, Gaddag::Node node) {
  const int idx = anchor_ + pos;
  const Square& s = sq_[idx];
  if (s.letter != Board::kEmpty) {
    std::uint8_t L = LetterOf(s.letter);
    Gaddag::Node next = gaddag_->Next(node, L);
    if (next != Gaddag::kNone) GoOn(pos, L, false, next);
    return;
  }
  if (rack_.Empty()) return;
  const bool has_blank = rack_letters_ & 1u;
  const std::uint32_t playable = has_blank ? kAllLetters : rack_letters_;
  for (std::uint32_t bits = s.cross & playable & gaddag_->LetterMask(node); bits;
       bits &= bits - 1) {
    const auto L = static_cast<std::uint8_t>(std::countr_zero(bits));
    const bool has_letter = rack_.Count(L) > 0;
    Gaddag::Node next = gaddag_->Next(node, L);
    ++tiles_placed_;
    if (has_letter) {
      Take(L);
      placed_[idx] = static_cast<char>('A' + L - 1);
      placed_value_[idx] = kScores[L];
      GoOn(pos, L, false, next);
      PutBack(L);
    }
    if (has_blank) {
      Take(kBlank);
      placed_[idx] = static_cast<char>('a' + L - 1);
      placed_value_[idx] = 0;
      GoOn(pos, L, true, next);
      PutBack(kBlank);
    }
    placed_[idx] = 0;
    --tiles_placed_;
  }
}

void MoveGenerator::GoOn(int pos, std::uint8_t, bool, Gaddag::Node next) {
  const int idx = anchor_ + pos;
  auto empty_at = [&](int i) { return i < 0 || i >= kBoardDim || sq_[i].letter == Board::kEmpty; };
  if (pos <= 0) {
    if (gaddag_->IsTerminal(next) && empty_at(idx - 1) && empty_at(anchor_ + 1))
      Record(idx, anchor_);
    // Extend left, but never onto an empty anchor: that move belongs to it.
    if (idx > 0 && !(sq_[idx - 1].letter == Board::kEmpty && sq_[idx - 1].anchor))
      Gen(pos - 1, next);
    Gaddag::Node sep = gaddag_->Next(next, Gaddag::kSeparator);
    if (sep != Gaddag::kNone && empty_at(idx - 1) && anchor_ + 1 < kBoardDim) {
      int saved = left_;
      left_ = idx;
      Gen(1, sep);
      left_ = saved;
    }
  } else {
    if (gaddag_->IsTerminal(next) && empty_at(idx + 1)) Record(left_, idx);
    if (idx + 1 < kBoardDim) Gen(pos + 1, next);
  }
}

void MoveGenerator::Record(int start, int end) {
  // A single tile with neighbours across the line was already found in the other direction.
  if (dir_ == Direction::kVertical && tiles_placed_ == 1) {
    for (int i = start; i <= end; ++i)
      if (placed_[i] && sq_[i].has_cross) return;
  }
  int main = 0, word_mult = 1, cross_total = 0;
  for (int i = start; i <= end; ++i) {
    const Square& q = sq_[i];
    if (!placed_[i]) {
      main += CellValue(q.letter);
      continue;
    }
    const int v = placed_value_[i] * q.letter_mult;
    main += v;
    word_mult *= q.word_mult;
    if (q.has_cross) cross_total += (q.cross_score + v) * q.word_mult;
  }
  const int score = main * word_mult + cross_total + (tiles_placed_ == kRackSize ? kBingoBonus : 0);
  if (!eval_) {
    out_->push_back(BuildMove(start, end, score));
    return;
  }
  ++seen_;
  // leave_index_ numbers exactly the tiles this placement leaves.
  const double value = score + leave_value_[leave_index_];
  if (found_ && value < *best_value_) return;
  Move m = BuildMove(start, end, score);
  if (!found_ || value > *best_value_ || TiebreaksBetter(m, *best_)) {
    *best_ = std::move(m);
    *best_value_ = value;
    found_ = true;
  }
}

Move MoveGenerator::BuildMove(int start, int end, int score) const {
  Move m;
  m.kind = Move::Kind::kPlace;
  m.dir = dir_;
  ToBoard(dir_, line_, start, m.row, m.col);
  m.tiles.reserve(end - start + 1);
  for (int i = start; i <= end; ++i) m.tiles.push_back(placed_[i] ? placed_[i] : '.');
  m.score = score;
  return m;
}

void MoveGenerator::ForEachRun(int rack_size,
                               const std::function<void(int l, int r, int k)>& fn) const {
  auto empty = [&](int i) { return sq_[i].letter == Board::kEmpty; };
  const std::uint32_t playable = (rack_letters_ & 1u) ? kAllLetters : rack_letters_;
  for (int l = 0; l < kBoardDim; ++l) {
    if (l > 0 && !empty(l - 1)) continue;  // a run starts after an empty square
    int k = 0;
    bool anchor = false;
    for (int r = l; r < kBoardDim; ++r) {
      if (empty(r)) {
        // No rack tile fits here, so no run reaches past it.
        if (!(sq_[r].cross & playable)) break;
        if (++k > rack_size) break;
        anchor = anchor || sq_[r].anchor;
      }
      if (r == l || k == 0 || !anchor) continue;
      if (r + 1 < kBoardDim && !empty(r + 1)) continue;  // the word goes on
      fn(l, r, k);
    }
  }
}

double MoveGenerator::RunBound(int l, int r, int k, const int* tile_values,
                               const double* best_leave) const {
  int existing = 0, word_mult = 1, cross_const = 0, m = 0, board_tiles = 0;
  int lm[kBoardDim], wm[kBoardDim];
  bool cross[kBoardDim];
  for (int i = l; i <= r; ++i) {
    if (sq_[i].letter != Board::kEmpty) {
      existing += CellValue(sq_[i].letter);
      ++board_tiles;
      continue;
    }
    lm[m] = sq_[i].letter_mult;
    wm[m] = sq_[i].word_mult;
    cross[m] = sq_[i].has_cross;
    word_mult *= wm[m];
    if (cross[m]) cross_const += sq_[i].cross_score * wm[m];
    ++m;
  }
  if (k == kRackSize && board_tiles <= 1 && !bingo_[board_tiles]) return -1e300;
  int coef[kBoardDim];
  for (int j = 0; j < m; ++j) coef[j] = lm[j] * word_mult + (cross[j] ? lm[j] * wm[j] : 0);
  SortDescending(coef, m);
  int score = existing * word_mult + cross_const + (k == kRackSize ? kBingoBonus : 0);
  for (int j = 0; j < k; ++j) score += tile_values[j] * coef[j];
  return score + best_leave[k];
}

void MoveGenerator::GenRun(int l, int r, int k) {
  run_l_ = l;
  run_r_ = r;
  run_k_ = k;
  run_key_ = AnagramIndex::Key();
  int m = 0, existing = 0, word_mult = 1, cross_const = 0;
  int lm[kBoardDim], wm[kBoardDim];
  bool cross[kBoardDim];
  for (int i = l; i <= r; ++i) {
    if (sq_[i].letter != Board::kEmpty) {
      run_key_.Add(LetterOf(sq_[i].letter));
      existing += CellValue(sq_[i].letter);
      continue;
    }
    run_empty_[m] = i;
    lm[m] = sq_[i].letter_mult;
    wm[m] = sq_[i].word_mult;
    cross[m] = sq_[i].has_cross;
    word_mult *= wm[m];
    if (cross[m]) cross_const += sq_[i].cross_score * wm[m];
    ++m;
  }
  for (int j = 0; j < m; ++j) run_coef_[j] = lm[j] * word_mult + (cross[j] ? lm[j] * wm[j] : 0);
  SortDescending(run_coef_, m);
  run_const_ = existing * word_mult + cross_const + (k == kRackSize ? kBingoBonus : 0);
  // The rack's distinct tiles, to take sub-racks of k tiles from.
  sub_n_ = 0;
  for (int t = 0; t < kNumTileTypes; ++t) {
    const int c = rack_.Count(Tile(t));
    if (c == 0) continue;
    sub_tile_[sub_n_] = Tile(t);
    sub_count_[sub_n_] = c;
    sub_take_[sub_n_] = 0;
    ++sub_n_;
  }
  TakeSubracks(0, k);
}

void MoveGenerator::TakeSubracks(int j, int left) {
  if (j == sub_n_) {
    if (left != 0) return;
    // A sub-rack of run_k_ tiles: its naturals with the run's board letters
    // (and a letter for each blank) are the letters of the words to place.
    AnagramIndex::Key key = run_key_;
    int blanks = 0, index = 0, values[kRackSize], nv = 0;
    for (int t = 0; t < kNumTileTypes; ++t) nat_left_[t] = 0;
    for (int i = 0; i < sub_n_; ++i) {
      const Tile t = sub_tile_[i];
      if (t == kBlank) {
        blanks = sub_take_[i];
      } else {
        nat_left_[t] = sub_take_[i];
        for (int c = 0; c < sub_take_[i]; ++c) {
          key.Add(t);
          values[nv++] = kScores[t];
        }
      }
      index += (sub_count_[i] - sub_take_[i]) * radix_[t];
    }
    if (eval_) {
      // Its tiles' best score here plus the exact value of what it leaves.
      SortDescending(values, nv);
      int score = run_const_;
      for (int i = 0; i < nv; ++i) score += values[i] * run_coef_[i];
      if (found_ && score + leave_value_[index] < *best_value_) return;
      leave_index_ = index;
    }
    blanks_left_ = blanks;
    const int length = run_r_ - run_l_ + 1;
    auto try_key = [&](const AnagramIndex::Key& k) {
      int count = 0;
      const std::uint8_t* words = index_->Find(k, length, count);
      for (int w = 0; w < count; ++w) PlaceWord(words + w * length, 0);
    };
    // Each blank stands for a letter: every multiset of them, in order.
    auto blank_letters = [&](auto& self, int left_blanks, std::uint8_t from) -> void {
      if (left_blanks == 0) {
        try_key(key);
        return;
      }
      for (std::uint8_t x = from; x <= 26; ++x) {
        key.Add(x);
        self(self, left_blanks - 1, x);
        key.Remove(x);
      }
    };
    blank_letters(blank_letters, blanks, 1);
    return;
  }
  for (int take = std::min(sub_count_[j], left); take >= 0; --take) {
    sub_take_[j] = take;
    TakeSubracks(j + 1, left - take);
  }
  sub_take_[j] = 0;
}

// Places `word` on the run, empty square by empty square from the i-th: each
// takes a natural tile or a blank whose letter its cross set allows.
void MoveGenerator::PlaceWord(const std::uint8_t* word, int i) {
  if (i == 0) {
    for (int s = run_l_; s <= run_r_; ++s)
      if (sq_[s].letter != Board::kEmpty && LetterOf(sq_[s].letter) != word[s - run_l_]) return;
  }
  if (i == run_k_) {
    tiles_placed_ = run_k_;
    Record(run_l_, run_r_);
    return;
  }
  const int s = run_empty_[i];
  const std::uint8_t c = word[s - run_l_];
  if (!(sq_[s].cross & (1u << c))) return;
  if (nat_left_[c] > 0) {
    --nat_left_[c];
    placed_[s] = static_cast<char>('A' + c - 1);
    placed_value_[s] = kScores[c];
    PlaceWord(word, i + 1);
    ++nat_left_[c];
  }
  if (blanks_left_ > 0) {
    --blanks_left_;
    placed_[s] = static_cast<char>('a' + c - 1);
    placed_value_[s] = 0;
    PlaceWord(word, i + 1);
    ++blanks_left_;
  }
  placed_[s] = 0;
}

}  // namespace scrabble
