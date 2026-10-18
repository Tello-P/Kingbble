#include "scrabble/endgame.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <limits>
#include <stdexcept>

namespace scrabble {

namespace {

constexpr int kInf = std::numeric_limits<int>::max() / 2;
constexpr int kExactDepth = 1 << 20;
constexpr std::uint8_t kExactBound = 0, kLower = 1, kUpper = 2;
constexpr int kMaxScoreless = 2;
// Bounds the table's memory (roughly 100 bytes per entry); once full, only
// positions already in it are updated.
constexpr std::size_t kMaxTableEntries = 1 << 21;

double Now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::uint64_t Mix(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

// Zobrist keys: one per (square, cell value), per (player, tile, count) and per side/scoreless.
std::uint64_t SquareKey(int sq, std::uint8_t cell) { return Mix(1000003ULL * sq + cell + 1); }
std::uint64_t RackKey(int player, int tile, int count) {
  return Mix(0xABCDEF0000ULL + 4096ULL * player + 64ULL * tile + count);
}

bool IsBlankChar(char ch) { return std::islower(static_cast<unsigned char>(ch)); }

}  // namespace

EndgameSolver::EndgameSolver(const Gaddag& gaddag) : gen_(gaddag) {}

std::uint64_t EndgameSolver::Hash() const {
  std::uint64_t h = Mix(0x5151515151ULL + static_cast<std::uint64_t>(to_move_ * 4 + scoreless_));
  for (int r = 0; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c)
      if (!board_.IsEmpty(r, c)) h ^= SquareKey(r * kBoardDim + c, board_.At(r, c));
  for (int p = 0; p < 2; ++p)
    for (int t = 0; t < kNumTileTypes; ++t)
      if (racks_[p].Count(Tile(t))) h ^= RackKey(p, t, racks_[p].Count(Tile(t)));
  return h;
}

int EndgameSolver::Play(const Move& m) {
  int score = 0;
  if (m.kind == Move::Kind::kPlace) {
    score = ScoreMove(board_, m);
    const int dr = m.dir == Direction::kVertical ? 1 : 0;
    const int dc = 1 - dr;
    for (std::size_t k = 0; k < m.tiles.size(); ++k) {
      char ch = m.tiles[k];
      if (ch == '.') continue;
      bool blank = IsBlankChar(ch);
      Tile letter = *TileFromChar(ch);
      racks_[to_move_].Remove(blank ? kBlank : letter);
      board_.Place(m.row + dr * static_cast<int>(k), m.col + dc * static_cast<int>(k), letter,
                   blank);
    }
  }
  scoreless_ = score > 0 ? 0 : scoreless_ + 1;
  to_move_ = 1 - to_move_;
  return score;
}

void EndgameSolver::Undo(const Move& m, int prev_scoreless) {
  to_move_ = 1 - to_move_;
  scoreless_ = prev_scoreless;
  if (m.kind != Move::Kind::kPlace) return;
  const int dr = m.dir == Direction::kVertical ? 1 : 0;
  const int dc = 1 - dr;
  for (std::size_t k = 0; k < m.tiles.size(); ++k) {
    char ch = m.tiles[k];
    if (ch == '.') continue;
    board_.Clear(m.row + dr * static_cast<int>(k), m.col + dc * static_cast<int>(k));
    racks_[to_move_].Add(IsBlankChar(ch) ? kBlank : *TileFromChar(ch));
  }
}

std::vector<Move> EndgameSolver::OrderedMoves(const Move* tt_move) {
  const Rack& rack = racks_[to_move_];
  std::vector<Move> moves = gen_.Generate(board_, rack);
  moves.emplace_back();  // pass
  auto key = [&](const Move& m) {
    // Going out first, then by score; the pass last.
    if (m.kind == Move::Kind::kPass) return -1;
    return (m.TilesPlayed() == rack.Size() ? 1000 : 0) + m.score;
  };
  std::stable_sort(moves.begin(), moves.end(),
                   [&](const Move& a, const Move& b) { return key(a) > key(b); });
  if (tt_move) {
    auto it = std::find(moves.begin(), moves.end(), *tt_move);
    if (it != moves.end()) std::rotate(moves.begin(), it, it + 1);
  }
  return moves;
}

int EndgameSolver::Search(int depth, int alpha, int beta, bool& complete) {
  ++nodes_;
  if (deadline_ > 0 && (nodes_ & 255) == 0 && Now() > deadline_) aborted_ = true;
  if (aborted_) return 0;

  const int me = to_move_;
  if (depth == 0) {
    complete = false;
    return RackValue(racks_[1 - me]) - RackValue(racks_[me]);
  }

  const std::uint64_t key = Hash();
  const Move* tt_move = nullptr;
  if (auto it = tt_.find(key); it != tt_.end()) {
    const Entry& e = it->second;
    if (e.depth >= depth && (e.bound == kExactBound || (e.bound == kLower && e.value >= beta) ||
                             (e.bound == kUpper && e.value <= alpha))) {
      if (e.depth < kExactDepth) complete = false;
      return e.value;
    }
    tt_move = &e.best;
  }

  const int alpha0 = alpha;
  int best = -kInf;
  Move best_move;
  bool all_complete = true;
  std::vector<Move> moves = OrderedMoves(tt_move);
  for (const Move& m : moves) {
    const int prev_scoreless = scoreless_;
    const int gained = Play(m);
    int value;
    bool child_complete = true;
    if (racks_[me].Empty()) {
      value = gained + 2 * RackValue(racks_[1 - me]);
    } else if (scoreless_ >= kMaxScoreless) {
      value = gained - RackValue(racks_[me]) + RackValue(racks_[1 - me]);
    } else {
      // value = gained - child, so the child's window is shifted by `gained`.
      // Principal variation search: after the first move, prove with a null
      // window that a move cannot beat the best so far, re-searching if it can.
      const int a = std::max(alpha, best);
      if (best == -kInf) {
        value = gained - Search(depth - 1, gained - beta, gained - a, child_complete);
      } else {
        value = gained - Search(depth - 1, gained - a - 1, gained - a, child_complete);
        if (value > a && value < beta && !aborted_) {
          child_complete = true;
          value = gained - Search(depth - 1, gained - beta, gained - a, child_complete);
        }
      }
    }
    Undo(m, prev_scoreless);
    if (aborted_) return 0;
    all_complete = all_complete && child_complete;
    if (value > best) {
      best = value;
      best_move = m;
    }
    if (best >= beta) break;
    alpha = std::max(alpha, best);
  }

  if (!all_complete) complete = false;
  Entry e{all_complete ? kExactDepth : depth, best,
          best <= alpha0 ? kUpper
          : best >= beta ? kLower
                         : kExactBound,
          best_move};
  if (key == root_key_) root_best_ = e.best;
  if (auto it = tt_.find(key); it != tt_.end()) {
    it->second = std::move(e);
  } else if (tt_.size() < kMaxTableEntries) {
    tt_.emplace(key, std::move(e));
  }
  return best;
}

void EndgameSolver::Load(const Game& game) {
  if (game.bag_size() != 0 || game.over())
    throw std::logic_error("endgame solver needs an empty bag");
  board_ = game.board();
  racks_ = {game.rack(0), game.rack(1)};
  to_move_ = game.to_move();
  // As Macondo: one scoreless turn left only if the game was one away from ending.
  scoreless_ = game.scoreless_turns() + 1 >= Game::kMaxScorelessTurns ? 1 : 0;
  nodes_ = 0;
  aborted_ = false;
  deadline_ = 0;
  root_key_ = Hash();
}

int EndgameSolver::SolveWindow(const Game& game, int alpha, int beta, int max_plies,
                               double time_limit_s) {
  // A full table stops learning; entries of earlier positions matter least.
  if (tt_.size() >= kMaxTableEntries) tt_.clear();
  Load(game);
  if (time_limit_s > 0) deadline_ = Now() + time_limit_s;
  int value = 0;
  for (int depth = 1; depth <= std::max(1, max_plies) && !aborted_; ++depth) {
    bool complete = true;
    value = Search(depth, alpha, beta, complete);
    if (complete) break;
  }
  return value;
}

EndgameResult EndgameSolver::Solve(const Game& game, int max_plies, double time_limit_s) {
  tt_.clear();
  Load(game);
  const double start = Now();

  EndgameResult result;
  for (int depth = 1; depth <= std::max(1, max_plies); ++depth) {
    if (depth > 1 && time_limit_s > 0) deadline_ = start + time_limit_s;
    bool complete = true;
    int value = Search(depth, -kInf, kInf, complete);
    if (aborted_) break;
    result.value = value;
    result.depth = depth;
    result.exact = complete;
    result.move = root_best_;
    if (complete) break;
  }
  result.nodes = nodes_;

  // Principal variation from the table.
  std::vector<std::pair<Move, int>> played;
  for (int i = 0; i < result.depth; ++i) {
    auto it = tt_.find(Hash());
    if (it == tt_.end()) break;
    Move m = it->second.best;
    const int me = to_move_;
    const int prev = scoreless_;
    Play(m);
    played.emplace_back(m, prev);
    result.pv.push_back(m);
    if (racks_[me].Empty() || scoreless_ >= kMaxScoreless) break;
  }
  for (auto it = played.rbegin(); it != played.rend(); ++it) Undo(it->first, it->second);
  return result;
}

}  // namespace scrabble
