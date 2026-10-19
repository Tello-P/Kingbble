#include "scrabble/movegen.hpp"

#include <algorithm>
#include <bit>
#include <cctype>

namespace scrabble {

namespace {

constexpr std::uint32_t kAllLetters = 0x7FFFFFE;  // bits 1..26

std::uint8_t CellAt(const Board& b, Direction dir, int line, int i) {
  return dir == Direction::kHorizontal ? b.At(line, i) : b.At(i, line);
}

std::uint8_t LetterOf(std::uint8_t cell) { return cell & ~Board::kBlankBit; }

int CellValue(std::uint8_t cell) {
  return (cell & Board::kBlankBit) ? 0 : EnglishTiles()[LetterOf(cell)].score;
}

int LetterMultiplier(Bonus b) {
  return b == Bonus::kDoubleLetter ? 2 : b == Bonus::kTripleLetter ? 3 : 1;
}

int WordMultiplier(Bonus b) {
  return b == Bonus::kDoubleWord ? 2 : b == Bonus::kTripleWord ? 3 : 1;
}

void ToBoard(Direction dir, int line, int i, int& row, int& col) {
  row = dir == Direction::kHorizontal ? line : i;
  col = dir == Direction::kHorizontal ? i : line;
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
      GenLine(dir, line);
    }
  }
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
      placed_[idx] = TileToChar(L);
      GoOn(pos, L, false, next);
      PutBack(L);
    }
    if (has_blank) {
      Take(kBlank);
      placed_[idx] = static_cast<char>(std::tolower(TileToChar(L)));
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
  Move m;
  m.kind = Move::Kind::kPlace;
  m.dir = dir_;
  ToBoard(dir_, line_, start, m.row, m.col);
  m.tiles.reserve(end - start + 1);
  int main = 0, word_mult = 1, cross_total = 0;
  for (int i = start; i <= end; ++i) {
    if (!placed_[i]) {
      m.tiles.push_back('.');
      main += CellValue(sq_[i].letter);
      continue;
    }
    char ch = placed_[i];
    m.tiles.push_back(ch);
    int r, c;
    ToBoard(dir_, line_, i, r, c);
    int v =
        std::islower(static_cast<unsigned char>(ch)) ? 0 : EnglishTiles()[*TileFromChar(ch)].score;
    Bonus bonus = Board::BonusAt(r, c);
    int lm = LetterMultiplier(bonus), wm = WordMultiplier(bonus);
    main += v * lm;
    word_mult *= wm;
    if (sq_[i].has_cross) cross_total += (sq_[i].cross_score + v * lm) * wm;
  }
  m.score = main * word_mult + cross_total + (tiles_placed_ == kRackSize ? kBingoBonus : 0);
  out_->push_back(std::move(m));
}

}  // namespace scrabble
