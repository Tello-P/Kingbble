#include "scrabble/features.hpp"

#include <algorithm>
#include <cmath>
#include <set>
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
  static const std::array<const char*, kNumMoveFeatures> names = {
      "gap", "score", "leave", "log_rank", "tiles", "bingo", "exchange", "pass"};
  return names;
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
  for (std::size_t i = 0; i < game.events.size(); ++i) {
    const GcgEvent& ev = game.events[i];
    if (ev.kind == GcgEvent::Kind::kPhonyWithdrawn && !turns.empty()) turns.back().phony = true;
    if (ev.kind != GcgEvent::Kind::kPlace && ev.kind != GcgEvent::Kind::kExchange &&
        ev.kind != GcgEvent::Kind::kPass)
      continue;
    ReplayedTurn t;
    t.turn = static_cast<int>(turns.size());
    t.player = ev.player;
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
