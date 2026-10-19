#include "scrabble/wordprune.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <unordered_set>

namespace scrabble {

namespace {

constexpr std::uint32_t kAllLetters = ((1u << 27) - 1) & ~1u;  // bits 1..26

class Pruner {
 public:
  Pruner(const Gaddag& gaddag, const Rack& pool) : g_(gaddag) {
    for (int t = 0; t < kNumTileTypes; ++t) left_[t] = pool.Count(Tile(t));
  }

  // Words ending at `end` of a line: the separator-free GADDAG paths are the
  // reversed words, so the line is read from `end` leftwards.
  void FromEnd(const std::array<std::uint8_t, kBoardDim>& cells, int end) {
    cells_ = &cells;
    Walk(end, g_.Root(), 0);
  }

  std::unordered_set<std::string>& words() { return words_; }

 private:
  void Walk(int i, Gaddag::Node node, int used) {
    const std::uint8_t cell = (*cells_)[i];
    if (cell != Board::kEmpty) {
      Step(i, cell & ~Board::kBlankBit, g_.Next(node, cell & ~Board::kBlankBit), used);
      return;
    }
    std::uint32_t allowed = 0;
    for (std::uint32_t bits = g_.LetterMask(node) & kAllLetters; bits; bits &= bits - 1) {
      const int L = std::countr_zero(bits);
      if (left_[L] > 0 || left_[kBlank] > 0) allowed |= 1u << L;
    }
    for (; allowed; allowed &= allowed - 1) {
      const auto L = static_cast<std::uint8_t>(std::countr_zero(allowed));
      const Tile used_tile = left_[L] > 0 ? L : kBlank;
      --left_[used_tile];
      Step(i, L, g_.Next(node, L), used + 1);
      ++left_[used_tile];
    }
  }

  void Step(int i, std::uint8_t letter, Gaddag::Node next, int used) {
    if (next == Gaddag::kNone) return;
    rev_.push_back(static_cast<char>('A' + letter - 1));
    const bool open_left = i == 0 || (*cells_)[i - 1] == Board::kEmpty;
    if (used > 0 && open_left && g_.IsTerminal(next)) words_.emplace(rev_.rbegin(), rev_.rend());
    if (i > 0) Walk(i - 1, next, used);
    rev_.pop_back();
  }

  const Gaddag& g_;
  std::array<int, kNumTileTypes> left_{};
  const std::array<std::uint8_t, kBoardDim>* cells_ = nullptr;
  std::string rev_;
  std::unordered_set<std::string> words_;
};

}  // namespace

std::vector<std::string> PlayableWords(const Gaddag& gaddag, const Board& board, const Rack& pool) {
  Pruner pruner(gaddag, pool);
  // Lines with the same squares up to a word's end give the same words (empty
  // lines above all): walk each of those prefixes once.
  std::unordered_set<std::string> walked;
  for (int dir = 0; dir < 2; ++dir) {
    for (int line = 0; line < kBoardDim; ++line) {
      std::array<std::uint8_t, kBoardDim> cells;
      for (int i = 0; i < kBoardDim; ++i)
        cells[i] = dir == 0 ? board.At(line, i) : board.At(i, line);
      for (int end = 0; end < kBoardDim; ++end) {
        if (end + 1 < kBoardDim && cells[end + 1] != Board::kEmpty) continue;
        if (!walked.emplace(reinterpret_cast<const char*>(cells.data()), end + 1).second) continue;
        pruner.FromEnd(cells, end);
      }
    }
  }
  std::vector<std::string> out(pruner.words().begin(), pruner.words().end());
  std::sort(out.begin(), out.end());
  return out;
}

Gaddag PruneLexicon(const Gaddag& gaddag, const Board& board, const Rack& pool) {
  return Gaddag::FromWords(PlayableWords(gaddag, board, pool));
}

}  // namespace scrabble
