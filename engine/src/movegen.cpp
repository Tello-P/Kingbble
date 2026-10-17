#include "scrabble/movegen.hpp"

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

std::vector<Move> MoveGenerator::Generate(const Board& board, const Rack& rack) {
  std::vector<Move> moves;
  board_ = &board;
  rack_ = rack;
  out_ = &moves;
  board_empty_ = board.IsEmpty(kCenter, kCenter);
  for (Direction dir : {Direction::kHorizontal, Direction::kVertical}) {
    for (int line = 0; line < kBoardDim; ++line) {
      PrepareLine(board, dir, line);
      GenLine(dir, line);
    }
  }
  return moves;
}

void MoveGenerator::PrepareLine(const Board& board, Direction dir, int line) {
  dir_ = dir;
  line_ = line;
  const Direction perp =
      dir == Direction::kHorizontal ? Direction::kVertical : Direction::kHorizontal;
  for (int i = 0; i < kBoardDim; ++i) {
    Square& s = sq_[i];
    s.letter = CellAt(board, dir, line, i);
    s.cross = kAllLetters;
    s.has_cross = false;
    s.cross_score = 0;
    s.anchor = false;
    if (s.letter != Board::kEmpty) continue;

    // Perpendicular neighbours: tiles before (lower index) and after this square.
    int lo = line, hi = line;
    while (lo > 0 && CellAt(board, perp, i, lo - 1) != Board::kEmpty) --lo;
    while (hi < kBoardDim - 1 && CellAt(board, perp, i, hi + 1) != Board::kEmpty) ++hi;
    bool side = (line > 0 && CellAt(board, dir, line - 1, i) != Board::kEmpty) ||
                (line < kBoardDim - 1 && CellAt(board, dir, line + 1, i) != Board::kEmpty);
    bool along = (i > 0 && sq_[i - 1].letter != Board::kEmpty) ||
                 (i < kBoardDim - 1 && CellAt(board, dir, line, i + 1) != Board::kEmpty);
    const bool opening_anchor = board_empty_ && line == kCenter && i == kCenter &&
                                (dir == Direction::kHorizontal || vertical_openings_);
    s.anchor = side || along || opening_anchor;
    if (lo == line && hi == line) continue;

    s.has_cross = true;
    for (int k = lo; k <= hi; ++k)
      if (k != line) s.cross_score += CellValue(CellAt(board, perp, i, k));
    // Lookup reverse(before + L + after) = reverse(after) + L + reverse(before).
    Gaddag::Node n = gaddag_.Root();
    for (int k = hi; k > line && n != Gaddag::kNone; --k)
      n = gaddag_.Next(n, LetterOf(CellAt(board, perp, i, k)));
    s.cross = 0;
    if (n == Gaddag::kNone) continue;
    for (std::uint8_t L = 1; L <= 26; ++L) {
      Gaddag::Node m = gaddag_.Next(n, L);
      for (int k = line - 1; k >= lo && m != Gaddag::kNone; --k)
        m = gaddag_.Next(m, LetterOf(CellAt(board, perp, i, k)));
      if (m != Gaddag::kNone && gaddag_.IsTerminal(m)) s.cross |= 1u << L;
    }
  }
}

void MoveGenerator::GenLine(Direction, int) {
  for (int i = 0; i < kBoardDim; ++i) placed_[i] = 0;
  for (int a = 0; a < kBoardDim; ++a) {
    if (!sq_[a].anchor) continue;
    anchor_ = a;
    tiles_placed_ = 0;
    Gen(0, gaddag_.Root());
  }
}

void MoveGenerator::Gen(int pos, Gaddag::Node node) {
  const int idx = anchor_ + pos;
  const Square& s = sq_[idx];
  if (s.letter != Board::kEmpty) {
    std::uint8_t L = LetterOf(s.letter);
    Gaddag::Node next = gaddag_.Next(node, L);
    if (next != Gaddag::kNone) GoOn(pos, L, false, next);
    return;
  }
  if (rack_.Empty()) return;
  const bool has_blank = rack_.Count(kBlank) > 0;
  for (std::uint8_t L = 1; L <= 26; ++L) {
    if (!(s.cross & (1u << L))) continue;
    const bool has_letter = rack_.Count(L) > 0;
    if (!has_letter && !has_blank) continue;
    Gaddag::Node next = gaddag_.Next(node, L);
    if (next == Gaddag::kNone) continue;
    ++tiles_placed_;
    if (has_letter) {
      rack_.Remove(L);
      placed_[idx] = TileToChar(L);
      GoOn(pos, L, false, next);
      rack_.Add(L);
    }
    if (has_blank) {
      rack_.Remove(kBlank);
      placed_[idx] = static_cast<char>(std::tolower(TileToChar(L)));
      GoOn(pos, L, true, next);
      rack_.Add(kBlank);
    }
    placed_[idx] = 0;
    --tiles_placed_;
  }
}

void MoveGenerator::GoOn(int pos, std::uint8_t, bool, Gaddag::Node next) {
  const int idx = anchor_ + pos;
  auto empty_at = [&](int i) { return i < 0 || i >= kBoardDim || sq_[i].letter == Board::kEmpty; };
  if (pos <= 0) {
    if (gaddag_.IsTerminal(next) && empty_at(idx - 1) && empty_at(anchor_ + 1))
      Record(idx, anchor_);
    // Extend left, but never onto an empty anchor: that move belongs to it.
    if (idx > 0 && !(sq_[idx - 1].letter == Board::kEmpty && sq_[idx - 1].anchor))
      Gen(pos - 1, next);
    Gaddag::Node sep = gaddag_.Next(next, Gaddag::kSeparator);
    if (sep != Gaddag::kNone && empty_at(idx - 1) && anchor_ + 1 < kBoardDim) {
      int saved = left_;
      left_ = idx;
      Gen(1, sep);
      left_ = saved;
    }
  } else {
    if (gaddag_.IsTerminal(next) && empty_at(idx + 1)) Record(left_, idx);
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
