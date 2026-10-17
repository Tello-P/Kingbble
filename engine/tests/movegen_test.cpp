#include "scrabble/movegen.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <tuple>

#include "scrabble/game.hpp"
#include "scrabble/greedy_bot.hpp"

namespace scrabble {

namespace {

std::vector<std::string> ReadWords(const std::string& path) {
  std::ifstream in(path);
  std::vector<std::string> words;
  std::string w;
  while (in >> w) {
    for (auto& c : w) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    words.push_back(w);
  }
  return words;
}

const std::vector<std::string>& SmallWords() {
  static const auto* w =
      new std::vector<std::string>(ReadWords(std::string(KINGBBLE_FIXTURES) + "/enable_small.txt"));
  return *w;
}

const Gaddag& SmallGaddag() {
  static const auto* g = new Gaddag(Gaddag::FromWords(SmallWords()));
  return *g;
}

bool Empty(const Board& b, int r, int c) {
  return r < 0 || r >= kBoardDim || c < 0 || c >= kBoardDim || b.IsEmpty(r, c);
}

char BoardChar(const Board& b, int r, int c) { return TileToChar(b.At(r, c) & ~Board::kBlankBit); }

// Reference generator: tries every word of the lexicon at every square and direction.
std::vector<Move> BruteForce(const Board& board, const Rack& rack, const Gaddag& g,
                             const std::vector<std::string>& words) {
  std::vector<Move> out;
  const bool board_empty = board.IsEmpty(kCenter, kCenter);
  for (Direction dir : {Direction::kHorizontal, Direction::kVertical}) {
    const int dr = dir == Direction::kVertical ? 1 : 0, dc = 1 - dr;
    for (int r0 = 0; r0 < kBoardDim; ++r0) {
      for (int c0 = 0; c0 < kBoardDim; ++c0) {
        if (!Empty(board, r0 - dr, c0 - dc)) continue;
        for (const std::string& w : words) {
          const int n = static_cast<int>(w.size());
          const int re = r0 + dr * (n - 1), ce = c0 + dc * (n - 1);
          if (re >= kBoardDim || ce >= kBoardDim || !Empty(board, re + dr, ce + dc)) continue;
          bool ok = true, connected = false, center = false;
          std::vector<int> fresh;
          for (int k = 0; k < n && ok; ++k) {
            int r = r0 + dr * k, c = c0 + dc * k;
            if (r == kCenter && c == kCenter) center = true;
            if (!board.IsEmpty(r, c)) {
              ok = BoardChar(board, r, c) == w[k];
              connected = true;
              continue;
            }
            // Perpendicular word through the new tile.
            std::string before, after;
            for (int rr = r - dc, cc = c - dr; !Empty(board, rr, cc); rr -= dc, cc -= dr)
              before.insert(before.begin(), BoardChar(board, rr, cc));
            for (int rr = r + dc, cc = c + dr; !Empty(board, rr, cc); rr += dc, cc += dr)
              after.push_back(BoardChar(board, rr, cc));
            if (!before.empty() || !after.empty()) {
              connected = true;
              ok = g.Contains(before + w[k] + after);
            }
            fresh.push_back(k);
          }
          if (!ok || fresh.empty() || !(board_empty ? center : connected)) continue;
          if (dir == Direction::kVertical && fresh.size() == 1) {
            int r = r0 + dr * fresh[0], c = c0 + dc * fresh[0];
            if (!Empty(board, r, c - 1) || !Empty(board, r, c + 1)) continue;
          }
          // Every assignment of rack letters or blanks to the new tiles.
          std::string tiles(n, '.');
          Rack left = rack;
          auto assign = [&](auto&& self, std::size_t i) -> void {
            if (i == fresh.size()) {
              Move m;
              m.kind = Move::Kind::kPlace;
              m.row = r0;
              m.col = c0;
              m.dir = dir;
              m.tiles = tiles;
              m.score = ScoreMove(board, m);
              out.push_back(m);
              return;
            }
            char ch = w[fresh[i]];
            Tile t = *TileFromChar(ch);
            if (left.Remove(t)) {
              tiles[fresh[i]] = ch;
              self(self, i + 1);
              left.Add(t);
            }
            if (left.Remove(kBlank)) {
              tiles[fresh[i]] = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
              self(self, i + 1);
              left.Add(kBlank);
            }
          };
          assign(assign, 0);
        }
      }
    }
  }
  return out;
}

void SortMoves(std::vector<Move>& v) {
  std::sort(v.begin(), v.end(), [](const Move& a, const Move& b) {
    return std::tie(a.dir, a.row, a.col, a.tiles) < std::tie(b.dir, b.row, b.col, b.tiles);
  });
}

}  // namespace

TEST(ScoreMove, FirstMoveOnCenterDoubleWord) {
  Board b;
  Move m{Move::Kind::kPlace, 7, 6, Direction::kHorizontal, "QI", 0};
  EXPECT_EQ(ScoreMove(b, m), 22);  // (10 + 1) x 2
}

TEST(ScoreMove, BlankScoresZeroAndBingoBonus) {
  Board b;
  Move m{Move::Kind::kPlace, 7, 1, Direction::kHorizontal, "RETAINs", 0};
  // T on the double letter at (7,3), blank s on the center double word: 7 x 2 + 50.
  EXPECT_EQ(ScoreMove(b, m), 64);
}

TEST(ScoreMove, CountsCrossWords) {
  Board b;
  b.Place(7, 7, *TileFromChar('Q'), false);
  // Vertical "QI" on the existing Q: I at (8,7) plus nothing else.
  Move m{Move::Kind::kPlace, 7, 7, Direction::kVertical, ".I", 0};
  EXPECT_EQ(ScoreMove(b, m), 11);
}

TEST(MoveGenerator, EmptyBoardMovesCoverCenter) {
  MoveGenerator gen(SmallGaddag());
  Board b;
  auto moves = gen.Generate(b, *Rack::FromString("QIAETRS"));
  ASSERT_FALSE(moves.empty());
  for (const Move& m : moves) {
    int n = static_cast<int>(m.tiles.size());
    bool covers = m.dir == Direction::kHorizontal
                      ? m.row == kCenter && m.col <= kCenter && m.col + n > kCenter
                      : m.col == kCenter && m.row <= kCenter && m.row + n > kCenter;
    EXPECT_TRUE(covers) << m.ToString();
  }
}

TEST(MoveGenerator, MatchesBruteForceDuringSelfPlay) {
  const Gaddag& g = SmallGaddag();
  MoveGenerator gen(g);
  GreedyBot bot(g);
  int positions = 0;
  for (std::uint64_t seed = 1; seed <= 3; ++seed) {
    Game game(seed);
    while (!game.over()) {
      const Rack& rack = game.rack(game.to_move());
      auto got = gen.Generate(game.board(), rack);
      auto want = BruteForce(game.board(), rack, g, SmallWords());
      SortMoves(got);
      SortMoves(want);
      ASSERT_EQ(got.size(), want.size()) << "seed " << seed << " rack " << rack.ToString();
      for (std::size_t i = 0; i < got.size(); ++i) {
        ASSERT_EQ(got[i], want[i]) << got[i].ToString() << " vs " << want[i].ToString();
      }
      // Blanks on the rack exercise both letter and blank designations.
      Rack with_blank = rack;
      if (with_blank.Size() > 0 && with_blank.Count(kBlank) == 0) {
        for (int t = 1; t < kNumTileTypes; ++t) {
          if (with_blank.Remove(Tile(t))) break;
        }
        with_blank.Add(kBlank);
        auto got_b = gen.Generate(game.board(), with_blank);
        auto want_b = BruteForce(game.board(), with_blank, g, SmallWords());
        SortMoves(got_b);
        SortMoves(want_b);
        ASSERT_EQ(got_b, want_b) << "seed " << seed << " rack " << with_blank.ToString();
      }
      ++positions;
      game.Apply(bot.Choose(game));
    }
  }
  EXPECT_GT(positions, 25);
}

}  // namespace scrabble
