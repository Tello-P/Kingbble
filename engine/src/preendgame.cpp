#include "scrabble/preendgame.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>

#include "scrabble/equity.hpp"
#include "scrabble/wordprune.hpp"

namespace scrabble {

namespace {

constexpr double kEps = 1e-9;
constexpr int kInf = std::numeric_limits<int>::max() / 4;

double Now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::uint64_t Mix(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

PegOutcome Sign(int spread) {
  return spread > 0 ? PegOutcome::kWin : spread == 0 ? PegOutcome::kDraw : PegOutcome::kLoss;
}

double WinPoints(PegOutcome o) {
  return o == PegOutcome::kWin ? 1 : o == PegOutcome::kDraw ? 0.5 : 0;
}

double LossPoints(PegOutcome o) {
  return o == PegOutcome::kLoss ? 1 : o == PegOutcome::kDraw ? 0.5 : 0;
}

std::string Signature(const Tile* tiles, int n) {
  return std::string(reinterpret_cast<const char*>(tiles), static_cast<std::size_t>(n));
}

// Draws that are the same for a play drawing `drawn` tiles: the order of those
// tiles does not matter, only the order of the ones left in the bag. Returns
// the merged draws and, for each input draw, the index of its group.
std::vector<PreEndgameSolver::Draw> Group(const std::vector<PreEndgameSolver::Draw>& draws,
                                          int drawn, std::vector<int>* index) {
  std::vector<PreEndgameSolver::Draw> groups;
  std::unordered_map<std::string, int> seen;
  if (index) index->clear();
  for (const auto& d : draws) {
    PreEndgameSolver::Draw key = d;
    // Insertion sort of at most kRackSize tiles (std::sort here trips GCC 13's
    // -Wstringop-overflow, which cannot see that the bound is at most 7).
    const int n = std::clamp(std::min(drawn, d.size), 0, kRackSize);
    for (int i = 1; i < n; ++i)
      for (int j = i; j > 0 && key.tiles[j - 1] > key.tiles[j]; --j)
        std::swap(key.tiles[j - 1], key.tiles[j]);
    auto [it, fresh] =
        seen.emplace(Signature(key.tiles.data(), key.size), static_cast<int>(groups.size()));
    if (fresh) {
      key.count = 0;
      groups.push_back(key);
    }
    groups[it->second].count += d.count;
    if (index) index->push_back(it->second);
  }
  return groups;
}

}  // namespace

PreEndgameSolver::PreEndgameSolver(const Gaddag& gaddag, const Leaves& leaves)
    : gaddag_(gaddag),
      gen_(gaddag),
      leaves_(leaves),
      endgame_(gaddag),
      own_shared_(std::make_unique<Shared>()),
      shared_(own_shared_.get()) {}

std::vector<PreEndgameSolver::Draw> PreEndgameSolver::Permutations(const Rack& pool, int k) {
  std::vector<Draw> out;
  std::array<int, kNumTileTypes> left{};
  for (int t = 0; t < kNumTileTypes; ++t) left[t] = pool.Count(Tile(t));
  Draw cur;
  cur.size = k;
  auto rec = [&](auto& self, int i, double count) -> void {
    if (i == k) {
      cur.count = count;
      out.push_back(cur);
      return;
    }
    for (int t = 0; t < kNumTileTypes; ++t) {
      if (left[t] == 0) continue;
      cur.tiles[i] = Tile(t);
      const int n = left[t]--;
      self(self, i + 1, count * n);
      ++left[t];
    }
  };
  rec(rec, 0, 1);
  return out;
}

void PreEndgameSolver::Deal(State& s, const Rack& unseen, const Draw& d) const {
  Rack opp = unseen;
  for (int i = 0; i < d.size; ++i) opp.Remove(d.tiles[i]);
  s.racks[1 - me_] = opp;
  s.bag = d.tiles;
  s.bag_next = 0;
  s.bag_size = d.size;
}

void PreEndgameSolver::Apply(State& s, const Move& m) const {
  Rack& rack = s.racks[s.to_move];
  int gained = 0;
  if (m.kind == Move::Kind::kPlace) {
    gained = ScoreMove(s.board, m);
    const int dr = m.dir == Direction::kVertical ? 1 : 0;
    const int dc = 1 - dr;
    for (std::size_t k = 0; k < m.tiles.size(); ++k) {
      const char ch = m.tiles[k];
      if (ch == '.') continue;
      const bool blank = std::islower(static_cast<unsigned char>(ch));
      const Tile letter = *TileFromChar(ch);
      rack.Remove(blank ? kBlank : letter);
      s.board.Place(m.row + dr * static_cast<int>(k), m.col + dc * static_cast<int>(k), letter,
                    blank);
    }
    while (rack.Size() < kRackSize && s.bag_next < s.bag_size) rack.Add(s.bag[s.bag_next++]);
  }
  const int sign = s.to_move == me_ ? 1 : -1;
  s.spread += sign * gained;
  s.scoreless = gained > 0 ? 0 : s.scoreless + 1;
  if (rack.Empty() && s.BagLeft() == 0) {
    s.spread += sign * 2 * RackValue(s.racks[1 - s.to_move]);
    s.over = true;
  } else if (s.scoreless >= 2) {
    s.spread -= RackValue(s.racks[me_]) - RackValue(s.racks[1 - me_]);
    s.over = true;
  }
  s.to_move = 1 - s.to_move;
}

double PreEndgameSolver::TimeLeft() const {
  // A tiny positive limit rather than 0, which would mean none.
  return deadline_ > 0 ? std::max(1e-9, deadline_ - Now()) : 0;
}

bool PreEndgameSolver::TimeUp() {
  if (deadline_ > 0 && !aborted_ && Now() > deadline_) aborted_ = true;
  return aborted_;
}

std::vector<Move> PreEndgameSolver::Replies(const State& s, bool with_pass) {
  std::vector<Move> moves = gen_.Generate(s.board, s.racks[s.to_move]);
  std::stable_sort(moves.begin(), moves.end(),
                   [](const Move& a, const Move& b) { return a.score > b.score; });
  // Passing back after a scoreless turn ends the game: cheap and often decisive.
  if (with_pass || moves.empty()) {
    if (s.scoreless > 0)
      moves.insert(moves.begin(), Move{});
    else
      moves.emplace_back();
  }
  return moves;
}

std::uint64_t PreEndgameSolver::EndgameKey(const State& s) const {
  std::uint64_t key =
      Mix(0xE9D0ULL + static_cast<std::uint64_t>(s.to_move * 2 + (s.scoreless > 0)));
  for (int r = 0; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c)
      if (!s.board.IsEmpty(r, c)) key ^= Mix(1000003ULL * (r * kBoardDim + c) + s.board.At(r, c));
  for (int p = 0; p < 2; ++p)
    for (int t = 0; t < kNumTileTypes; ++t)
      key ^= Mix(0xC0FFEE00ULL + 4096ULL * p + 64ULL * t + s.racks[p].Count(Tile(t)));
  return key;
}

int PreEndgameSolver::EndgameValue(const State& s, int alpha, int beta) {
  const std::uint64_t key = EndgameKey(s);
  {
    std::lock_guard<std::mutex> lock(shared_->mutex);
    if (auto it = shared_->endgames.find(key); it != shared_->endgames.end()) {
      const EndgameBounds& b = it->second;
      if (b.lo >= beta || b.hi <= alpha || b.lo == b.hi) return b.lo >= beta ? b.lo : b.hi;
    }
  }
  ++endgames_;
  // Game counts scoreless turns up to 6; the endgame solver maps 5 to "one left".
  Game g = Game::FromPosition(s.board, s.racks, {0, 0}, s.to_move, Rack{}, s.scoreless ? 5 : 0, 0);
  const int v = endgame_.SolveWindow(g, alpha, beta, params_.endgame_plies, TimeLeft());
  if (endgame_.aborted()) {
    aborted_ = true;
    return 0;
  }
  std::lock_guard<std::mutex> lock(shared_->mutex);
  EndgameBounds& b = shared_->endgames[key];
  if (v <= alpha) {
    b.hi = std::min(b.hi, v);
  } else if (v >= beta) {
    b.lo = std::max(b.lo, v);
  } else {
    b.lo = b.hi = v;
  }
  return v;
}

PegOutcome PreEndgameSolver::Endgame(const State& s) {
  // A window around the value that ties the game is enough to tell who wins.
  const int tie = s.to_move == me_ ? -s.spread : s.spread;
  const int v = EndgameValue(s, tie - 1, tie + 1);
  if (aborted_) return PegOutcome::kUnknown;
  return Sign(s.to_move == me_ ? s.spread + v : s.spread - v);
}

int PreEndgameSolver::FinalSpread(const State& s) {
  if (s.over) return s.spread;
  const int v = EndgameValue(s, -kInf, kInf);
  return s.to_move == me_ ? s.spread + v : s.spread - v;
}

int PreEndgameSolver::Hardness(const State& s) {
  if (s.over || s.to_move == me_) return s.spread;
  int best = 0;
  for (const Move& m : gen_.Generate(s.board, s.racks[s.to_move])) best = std::max(best, m.score);
  return s.spread - best;
}

PegOutcome PreEndgameSolver::Continue(const State& s, int nested) {
  if (TimeUp()) return PegOutcome::kUnknown;
  if (s.over) return Sign(s.spread);
  if (s.BagLeft() == 0) return Endgame(s);
  if (s.to_move != me_) {
    // Pessimistic: one reply that beats us is enough.
    PegOutcome worst = PegOutcome::kUnknown;
    for (const Move& m : Replies(s, true)) {
      State next = s;
      Apply(next, m);
      worst = std::max(worst, Continue(next, nested));
      if (worst == PegOutcome::kLoss || aborted_) break;
    }
    return worst;
  }
  if (params_.nested_depth_limit >= 0 && nested + 1 > params_.nested_depth_limit)
    return PegOutcome::kUnknown;
  return Nested(s, nested + 1);
}

PegOutcome PreEndgameSolver::Nested(const State& s, int nested) {
  const int opp = 1 - me_;
  Rack unseen = s.racks[opp];
  for (int i = s.bag_next; i < s.bag_size; ++i) unseen.Add(s.bag[i]);
  const int k = s.BagLeft();
  const std::string actual = Signature(s.bag.data() + s.bag_next, k);

  // What we know here: the board, our rack, the unseen tiles, the spread and
  // the scoreless turns.
  std::uint64_t key = Mix(0x9E57ULL + static_cast<std::uint64_t>(s.scoreless) * 131 +
                          static_cast<std::uint64_t>(s.spread + (1 << 20)) * 7919);
  for (int r = 0; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c)
      if (!s.board.IsEmpty(r, c)) key ^= Mix(1000003ULL * (r * kBoardDim + c) + s.board.At(r, c));
  for (int t = 0; t < kNumTileTypes; ++t) {
    key ^= Mix(0xA11CE000ULL + 64ULL * t + s.racks[me_].Count(Tile(t)));
    key ^= Mix(0xB0B00000ULL + 64ULL * t + unseen.Count(Tile(t)));
  }
  {
    std::lock_guard<std::mutex> lock(shared_->mutex);
    if (auto it = shared_->nested.find(key); it != shared_->nested.end()) {
      if (auto v = it->second.find(actual); v != it->second.end()) return v->second;
    }
  }

  const std::vector<Draw> draws = Permutations(unseen, k);
  const Rack& rack = s.racks[me_];
  std::vector<Move> moves = gen_.Generate(s.board, rack);
  if (!params_.skip_deep_pass || s.scoreless > 0 || moves.empty()) moves.emplace_back();
  std::vector<double> estimate;
  for (const Move& m : moves) estimate.push_back(m.score + leaves_.Value(LeaveAfter(rack, m)));
  std::vector<int> order(moves.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
  std::stable_sort(order.begin(), order.end(),
                   [&](int a, int b) { return estimate[a] > estimate[b]; });

  // Each play over every draw; a play is dropped once it has more losses than
  // the best so far.
  std::vector<std::vector<PegOutcome>> outcomes(moves.size());
  std::vector<double> losses(moves.size(), 0);
  std::vector<bool> dropped(moves.size(), false);
  double fewest = std::numeric_limits<double>::infinity();
  std::unordered_map<std::string, PegOutcome> verdicts;
  std::vector<int> group_of;
  for (int j : order) {
    const Move& m = moves[j];
    const std::vector<Draw> groups = Group(draws, std::min(m.TilesPlayed(), k), &group_of);
    std::vector<PegOutcome> by_group(groups.size(), PegOutcome::kUnknown);
    bool all_won = true;
    for (std::size_t g = 0; g < groups.size(); ++g) {
      State next = s;
      Deal(next, unseen, groups[g]);
      Apply(next, m);
      by_group[g] = Continue(next, nested);
      if (aborted_) return PegOutcome::kUnknown;
      all_won = all_won && by_group[g] == PegOutcome::kWin;
      losses[j] += groups[g].count * LossPoints(by_group[g]);
      if (losses[j] > fewest + kEps) {
        dropped[j] = true;
        break;
      }
    }
    if (dropped[j]) continue;
    outcomes[j].resize(draws.size());
    for (std::size_t i = 0; i < draws.size(); ++i) outcomes[j][i] = by_group[group_of[i]];
    fewest = std::min(fewest, losses[j]);
    if (all_won) {
      // Wins whatever the draw: no other play can do better.
      for (const Draw& d : draws) verdicts[Signature(d.tiles.data(), k)] = PegOutcome::kWin;
      std::lock_guard<std::mutex> lock(shared_->mutex);
      shared_->nested[key] = std::move(verdicts);
      return PegOutcome::kWin;
    }
  }

  // We cannot tell which of the plays with the fewest losses we would choose:
  // for each draw take the worst of them.
  for (std::size_t i = 0; i < draws.size(); ++i) {
    PegOutcome worst = PegOutcome::kUnknown;
    bool any = false;
    for (std::size_t j = 0; j < moves.size(); ++j) {
      if (dropped[j] || losses[j] > fewest + kEps) continue;
      any = true;
      worst = std::max(worst, outcomes[j][i]);
    }
    verdicts[Signature(draws[i].tiles.data(), k)] = any ? worst : PegOutcome::kLoss;
  }
  const PegOutcome result = verdicts[actual];
  std::lock_guard<std::mutex> lock(shared_->mutex);
  shared_->nested[key] = std::move(verdicts);
  return result;
}

std::vector<PreEndgameSolver*> PreEndgameSolver::Workers(int threads, const Gaddag& lexicon) {
  std::vector<PreEndgameSolver*> workers = {this};
  for (int i = 1; i < threads; ++i) {
    if (static_cast<int>(helpers_.size()) < i)
      helpers_.push_back(std::make_unique<PreEndgameSolver>(gaddag_, leaves_));
    PreEndgameSolver& w = *helpers_[i - 1];
    w.params_ = params_;
    w.me_ = me_;
    w.endgames_ = 0;
    w.deadline_ = deadline_;
    w.aborted_ = false;
    w.shared_ = shared_;
    w.gen_.set_gaddag(lexicon);
    w.endgame_.set_window_gaddag(lexicon);
    w.endgame_.ClearTable();
    workers.push_back(&w);
  }
  return workers;
}

void PreEndgameSolver::RunTasks(const std::vector<PreEndgameSolver*>& workers, std::size_t n,
                                const std::function<void(PreEndgameSolver&, std::size_t)>& task) {
  std::atomic<std::size_t> next{0};
  auto work = [&](PreEndgameSolver& w) {
    for (std::size_t i; !w.TimeUp() && (i = next++) < n;) task(w, i);
  };
  std::vector<std::thread> threads;
  for (std::size_t i = 1; i < workers.size(); ++i)
    threads.emplace_back(work, std::ref(*workers[i]));
  work(*workers[0]);
  for (std::thread& t : threads) t.join();
}

bool PreEndgameSolver::Aborted(const std::vector<PreEndgameSolver*>& workers) {
  return std::any_of(workers.begin(), workers.end(),
                     [](const PreEndgameSolver* w) { return w->aborted_; });
}

PreEndgameResult PreEndgameSolver::Solve(const Game& game, const PreEndgameParams& params) {
  const int k = game.bag_size();
  if (game.over() || k < 1 || k > 6)
    throw std::logic_error("pre-endgame needs 1 to 6 tiles in the bag");
  params_ = params;
  me_ = game.to_move();
  endgames_ = 0;
  aborted_ = false;
  deadline_ = params.time_limit_s > 0 ? Now() + params.time_limit_s : 0;
  shared_->nested.clear();
  shared_->endgames.clear();
  endgame_.ClearTable();

  const Rack& rack = game.rack(me_);
  const Rack unseen = UnseenTiles(game.board(), rack);
  // Every tile off the board: ours and the unseen ones.
  if (pruning_) pruned_ = PruneLexicon(gaddag_, game.board(), UnseenTiles(game.board(), Rack{}));
  const Gaddag& lexicon = pruning_ ? pruned_ : gaddag_;
  gen_.set_gaddag(lexicon);
  endgame_.set_window_gaddag(lexicon);
  std::vector<PreEndgameSolver*> workers = Workers(params.threads, lexicon);
  State root;
  root.board = game.board();
  root.racks[me_] = rack;
  root.spread = game.score(me_) - game.score(1 - me_);
  root.to_move = me_;
  // As Macondo's endgame mode: one scoreless turn left only if the game was one away from ending.
  root.scoreless = game.scoreless_turns() + 1 >= Game::kMaxScorelessTurns ? 1 : 0;

  const std::vector<Draw> draws = Permutations(unseen, k);
  PreEndgameResult result;
  for (const Draw& d : draws) result.permutations += d.count;

  std::vector<Move> moves = gen_.Generate(game.board(), rack);
  moves.emplace_back();  // pass
  for (Move& m : moves) {
    PreEndgamePlay p;
    p.equity = StaticEquity(m, rack, k, unseen, leaves_);
    p.move = std::move(m);
    result.plays.push_back(std::move(p));
  }
  std::sort(result.plays.begin(), result.plays.end(),
            [](const PreEndgamePlay& a, const PreEndgamePlay& b) {
              return a.equity != b.equity ? a.equity > b.equity : TiebreaksBetter(a.move, b.move);
            });

  // The candidates, each with its draws ordered hardest first.
  struct Candidate {
    PreEndgamePlay* play;
    std::vector<Draw> groups;
    PegOutcome first = PegOutcome::kUnknown;  // against groups[0]
  };
  std::vector<Candidate> candidates;
  int rank = 0;
  for (PreEndgamePlay& p : result.plays) {
    const int played = p.move.TilesPlayed();
    const bool candidate = params.candidates <= 0 || rank++ < params.candidates ||
                           (params.pass_candidate && p.move.kind == Move::Kind::kPass);
    if (!candidate || (params.max_tiles_left >= 0 && k - played > params.max_tiles_left)) {
      p.skipped = true;
      continue;
    }
    Candidate c{&p, Group(draws, std::min(played, k), nullptr)};
    std::vector<int> hardness;
    for (const Draw& g : c.groups) {
      State s = root;
      Deal(s, unseen, g);
      Apply(s, p.move);
      hardness.push_back(Hardness(s));
    }
    std::vector<int> order(c.groups.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return hardness[a] < hardness[b]; });
    std::vector<Draw> sorted;
    for (int i : order) sorted.push_back(c.groups[i]);
    c.groups = std::move(sorted);
    candidates.push_back(std::move(c));
  }

  // Each play against its hardest draw first: the plays that survive it are
  // analysed first, so a strong play soon bounds how many losses the others
  // may have.
  RunTasks(workers, candidates.size(), [&](PreEndgameSolver& w, std::size_t i) {
    Candidate& c = candidates[i];
    State s = root;
    Deal(s, unseen, c.groups[0]);
    Apply(s, c.play->move);
    c.first = w.Continue(s, 0);
  });
  std::stable_sort(
      candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return LossPoints(a.first) - WinPoints(a.first) < LossPoints(b.first) - WinPoints(b.first);
      });

  // Every draw of every play, in that order. Upper bound on the losses of the
  // play that will end up with the most wins.
  std::vector<std::pair<std::size_t, std::size_t>> tasks;
  for (std::size_t c = 0; c < candidates.size(); ++c)
    for (std::size_t g = 0; g < candidates[c].groups.size(); ++g) tasks.emplace_back(c, g);
  double fewest_possible = std::numeric_limits<double>::infinity();
  std::vector<std::size_t> done(candidates.size(), 0);  // draws finished or skipped
  std::mutex mutex;
  RunTasks(workers, tasks.size(), [&](PreEndgameSolver& w, std::size_t t) {
    const Candidate& c = candidates[tasks[t].first];
    const std::size_t i = tasks[t].second;
    PreEndgamePlay& p = *c.play;
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++done[tasks[t].first];
      if (p.cut) return;
      if (params.early_cutoff && p.losses > fewest_possible + kEps) {
        p.cut = true;
        return;
      }
    }
    PegOutcome o = c.first;
    if (i > 0) {
      State s = root;
      Deal(s, unseen, c.groups[i]);
      Apply(s, p.move);
      o = w.Continue(s, 0);
    }
    std::lock_guard<std::mutex> lock(mutex);
    if (w.aborted_) {
      p.cut = true;
      --done[tasks[t].first];
      return;
    }
    p.wins += c.groups[i].count * WinPoints(o);
    p.losses += c.groups[i].count * LossPoints(o);
    fewest_possible = std::min(fewest_possible, result.permutations - p.wins);
  });
  // Plays the time limit stopped short of are cut too.
  for (std::size_t c = 0; c < candidates.size(); ++c)
    if (done[c] < candidates[c].groups.size()) candidates[c].play->cut = true;
  result.complete = !Aborted(workers);

  auto analysed = [](const PreEndgamePlay& p) { return !p.skipped && !p.cut; };
  std::stable_sort(result.plays.begin(), result.plays.end(),
                   [&](const PreEndgamePlay& a, const PreEndgamePlay& b) {
                     if (analysed(a) != analysed(b)) return analysed(a);
                     return a.wins > b.wins + kEps;
                   });

  if (params.tiebreak && result.complete && !result.plays.empty() && analysed(result.plays[0])) {
    const double top = result.plays[0].wins;
    std::vector<std::size_t> tied;
    for (std::size_t i = 0; i < result.plays.size(); ++i) {
      const PreEndgamePlay& p = result.plays[i];
      if (analysed(p) && p.wins >= top - kEps && p.move.TilesPlayed() >= k) tied.push_back(i);
    }
    if (tied.size() == 1) {
      std::rotate(result.plays.begin(), result.plays.begin() + tied[0],
                  result.plays.begin() + tied[0] + 1);
    } else if (tied.size() > 1) {
      // Highest scoring first, as Macondo, then the final spread over every draw.
      std::stable_sort(tied.begin(), tied.end(), [&](std::size_t a, std::size_t b) {
        return result.plays[a].move.score > result.plays[b].move.score;
      });
      tied.resize(std::min(tied.size(), static_cast<std::size_t>(params.tiebreak_plays)));
      const std::vector<Draw> groups = Group(draws, k, nullptr);
      std::mutex mutex;
      RunTasks(workers, tied.size() * groups.size(), [&](PreEndgameSolver& w, std::size_t t) {
        PreEndgamePlay& p = result.plays[tied[t / groups.size()]];
        const Draw& g = groups[t % groups.size()];
        State s = root;
        Deal(s, unseen, g);
        Apply(s, p.move);
        const int spread = w.FinalSpread(s);
        std::lock_guard<std::mutex> lock(mutex);
        p.spread += g.count * spread;
        p.has_spread = true;
      });
      if (Aborted(workers)) {
        result.complete = false;
        for (std::size_t i : tied) result.plays[i].has_spread = false;
      } else {
        std::stable_sort(result.plays.begin(), result.plays.end(),
                         [](const PreEndgamePlay& a, const PreEndgamePlay& b) {
                           if (a.has_spread != b.has_spread) return a.has_spread;
                           return a.has_spread && a.spread > b.spread;
                         });
      }
    }
  }
  for (const PreEndgameSolver* w : workers) result.endgames += w->endgames_;
  return result;
}

}  // namespace scrabble
