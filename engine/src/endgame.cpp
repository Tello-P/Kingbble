#include "scrabble/endgame.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <limits>
#include <random>
#include <stdexcept>
#include <thread>

#include "scrabble/wordprune.hpp"

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

std::uint64_t SideKey(int to_move, int scoreless) {
  return Mix(0x5151515151ULL + static_cast<std::uint64_t>(to_move * 4 + scoreless));
}

bool IsBlankChar(char ch) { return std::islower(static_cast<unsigned char>(ch)); }

}  // namespace

EndgameSolver::EndgameSolver(const Gaddag& gaddag)
    : gaddag_(gaddag),
      window_gaddag_(&gaddag),
      gen_(gaddag),
      own_table_(std::make_unique<Table>()),
      tt_(own_table_.get()) {}

bool EndgameSolver::Table::Probe(std::uint64_t key, Entry& out) {
  Shard& s = ShardOf(key);
  std::lock_guard<std::mutex> lock(s.mutex);
  auto it = s.map.find(key);
  if (it == s.map.end()) return false;
  out = it->second;
  return true;
}

void EndgameSolver::Table::Store(std::uint64_t key, Entry e) {
  Shard& s = ShardOf(key);
  std::lock_guard<std::mutex> lock(s.mutex);
  if (auto it = s.map.find(key); it != s.map.end()) {
    it->second = std::move(e);
  } else if (size_ < kMaxTableEntries) {
    s.map.emplace(key, std::move(e));
    ++size_;
  }
}

void EndgameSolver::Table::Clear() {
  for (Shard& s : shards_) {
    std::lock_guard<std::mutex> lock(s.mutex);
    s.map.clear();
  }
  size_ = 0;
}

std::uint64_t EndgameSolver::Hash() const {
  std::uint64_t h = SideKey(to_move_, scoreless_);
  for (int r = 0; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c)
      if (!board_.IsEmpty(r, c)) h ^= SquareKey(r * kBoardDim + c, board_.At(r, c));
  for (int p = 0; p < 2; ++p)
    for (int t = 0; t < kNumTileTypes; ++t)
      if (racks_[p].Count(Tile(t))) h ^= RackKey(p, t, racks_[p].Count(Tile(t)));
  return h;
}

int EndgameSolver::Play(const Move& m) {
  hash_ ^= SideKey(to_move_, scoreless_);
  int score = 0;
  if (m.kind == Move::Kind::kPlace) {
    score = ScoreMove(board_, m);
    const int dr = m.dir == Direction::kVertical ? 1 : 0;
    const int dc = 1 - dr;
    Rack& rack = racks_[to_move_];
    for (std::size_t k = 0; k < m.tiles.size(); ++k) {
      char ch = m.tiles[k];
      if (ch == '.') continue;
      bool blank = IsBlankChar(ch);
      Tile letter = *TileFromChar(ch);
      const Tile t = blank ? kBlank : letter;
      const int n = rack.Count(t);
      hash_ ^= RackKey(to_move_, t, n);
      if (n > 1) hash_ ^= RackKey(to_move_, t, n - 1);
      rack.Remove(t);
      const int r = m.row + dr * static_cast<int>(k), c = m.col + dc * static_cast<int>(k);
      board_.Place(r, c, letter, blank);
      hash_ ^= SquareKey(r * kBoardDim + c, board_.At(r, c));
    }
  }
  scoreless_ = score > 0 ? 0 : scoreless_ + 1;
  to_move_ = 1 - to_move_;
  hash_ ^= SideKey(to_move_, scoreless_);
  return score;
}

void EndgameSolver::Undo(const Move& m, int prev_scoreless, std::uint64_t prev_hash) {
  hash_ = prev_hash;
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

void EndgameSolver::OrderedMoves(const Move* tt_move, std::vector<Move>& moves) {
  const Rack& rack = racks_[to_move_];
  gen_.Generate(board_, rack, moves);
  moves.emplace_back();  // pass
  // Going out first, then by score; the pass last. Ties keep generation order.
  order_.clear();
  for (std::size_t i = 0; i < moves.size(); ++i) {
    const Move& m = moves[i];
    const int key =
        m.kind == Move::Kind::kPass ? -1 : (m.TilesPlayed() == rack.Size() ? 1000 : 0) + m.score;
    order_.emplace_back(-key, static_cast<int>(i));
  }
  std::sort(order_.begin(), order_.end());
  sorted_.clear();
  for (const auto& [key, i] : order_) sorted_.push_back(std::move(moves[i]));
  moves.swap(sorted_);
  // Helpers try the moves after the first few in their own order, so that
  // they fill the shared table with other parts of the tree.
  if (helper_ > 0 && moves.size() > 3) {
    std::mt19937 rng(static_cast<std::uint32_t>(helper_ * 7919 + ply_));
    std::shuffle(moves.begin() + 2, moves.end(), rng);
  }
  if (tt_move) {
    auto it = std::find(moves.begin(), moves.end(), *tt_move);
    if (it != moves.end()) std::rotate(moves.begin(), it, it + 1);
  }
}

int EndgameSolver::Search(int depth, int alpha, int beta, bool& complete) {
  ++nodes_;
  if (stop_ && stop_->load(std::memory_order_relaxed)) aborted_ = true;
  if (deadline_ > 0 && (nodes_ & 255) == 0 && Now() > deadline_) aborted_ = true;
  if (aborted_) return 0;

  const int me = to_move_;
  if (depth == 0) {
    complete = false;
    return RackValue(racks_[1 - me]) - RackValue(racks_[me]);
  }

  const std::uint64_t key = hash_;
  const Move* tt_move = nullptr;
  Entry e;
  if (tt_->Probe(key, e)) {
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
  // One move list per ply, reused across calls.
  if (ply_ == static_cast<int>(move_lists_.size())) move_lists_.emplace_back();
  std::vector<Move>& moves = move_lists_[ply_];
  OrderedMoves(tt_move, moves);
  ++ply_;
  for (const Move& m : moves) {
    const int prev_scoreless = scoreless_;
    const std::uint64_t prev_hash = hash_;
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
    Undo(m, prev_scoreless, prev_hash);
    if (aborted_) {
      --ply_;
      return 0;
    }
    all_complete = all_complete && child_complete;
    if (value > best) {
      best = value;
      best_move = m;
    }
    if (best >= beta) break;
    alpha = std::max(alpha, best);
  }
  --ply_;

  if (!all_complete) complete = false;
  if (key == root_key_) root_best_ = best_move;
  tt_->Store(key, {all_complete ? kExactDepth : depth, best,
                   best <= alpha0 ? kUpper
                   : best >= beta ? kLower
                                  : kExactBound,
                   std::move(best_move)});
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
  hash_ = Hash();
  root_key_ = hash_;
  ply_ = 0;
}

int EndgameSolver::SolveWindow(const Game& game, int alpha, int beta, int max_plies,
                               double time_limit_s) {
  // A full table stops learning; entries of earlier positions matter least.
  if (tt_->Size() >= kMaxTableEntries) tt_->Clear();
  if (!gen_on_window_) {
    gen_.set_gaddag(*window_gaddag_);
    gen_on_window_ = true;
  }
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
  tt_->Clear();
  Load(game);
  const double start = Now();
  gen_on_window_ = false;
  if (pruning_) {
    Rack pool = racks_[0];
    for (int t = 0; t < kNumTileTypes; ++t)
      for (int n = 0; n < racks_[1].Count(Tile(t)); ++n) pool.Add(Tile(t));
    pruned_ = PruneLexicon(gaddag_, board_, pool);
    gen_.set_gaddag(pruned_);
  } else {
    gen_.set_gaddag(gaddag_);
  }

  // Helpers search the same position into the shared table, half of them one
  // ply deeper, until this search is done.
  std::atomic<bool> stop{false};
  std::vector<std::thread> threads;
  for (int i = 1; i < threads_; ++i) {
    if (static_cast<int>(helpers_.size()) < i)
      helpers_.push_back(std::make_unique<EndgameSolver>(gaddag_));
    EndgameSolver& h = *helpers_[i - 1];
    h.tt_ = tt_;
    h.stop_ = &stop;
    h.helper_ = i;
    h.gen_.set_gaddag(pruning_ ? pruned_ : gaddag_);
    h.gen_on_window_ = false;
    h.Load(game);
    threads.emplace_back([&h, i, max_plies] {
      for (int depth = 1 + i % 2; depth <= std::max(1, max_plies); ++depth) {
        bool complete = true;
        h.Search(depth, -kInf, kInf, complete);
        if (h.aborted_ || complete) break;
      }
    });
  }

  EndgameResult result;
  for (int depth = 1; depth <= std::max(1, max_plies); ++depth) {
    if (depth > 1 && time_limit_s > 0) deadline_ = start + time_limit_s;
    bool complete = true;
    int value = Search(depth, -kInf, kInf, complete);
    if (aborted_) break;
    result.value = value;
    result.depth = depth;
    result.exact = complete;
    // A helper may have stored the root first.
    Entry root;
    result.move = tt_->Probe(root_key_, root) ? root.best : root_best_;
    if (complete) break;
  }
  stop = true;
  for (std::thread& t : threads) t.join();
  result.nodes = nodes_;
  for (int i = 1; i < threads_; ++i) {
    result.nodes += helpers_[i - 1]->nodes_;
    helpers_[i - 1]->stop_ = nullptr;
  }

  // Principal variation from the table.
  struct Played {
    Move move;
    int scoreless;
    std::uint64_t hash;
  };
  std::vector<Played> played;
  for (int i = 0; i < result.depth; ++i) {
    Entry e;
    if (!tt_->Probe(hash_, e)) break;
    Move m = e.best;
    const int me = to_move_;
    played.push_back({m, scoreless_, hash_});
    Play(m);
    result.pv.push_back(m);
    if (racks_[me].Empty() || scoreless_ >= kMaxScoreless) break;
  }
  for (auto it = played.rbegin(); it != played.rend(); ++it)
    Undo(it->move, it->scoreless, it->hash);
  return result;
}

}  // namespace scrabble
