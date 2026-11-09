// One game against a bot, driven line by line on stdin with JSON replies on
// stdout (for the local web UI in app/web).
//
//   kingbble_play --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 [--bot equity|sim]
//                 [--sim-time S] [--threads N]
//
// Commands (one per line):
//   new SEED SEAT          new game, the human sits in SEAT (0 moves first)
//   state                  the current state
//   place R,C,L ...        the human places tiles: row and column 0-14, letter
//                          A-Z (lowercase for a blank played as that letter)
//   exchange TILES         e.g. "exchange AE?"
//   pass
//   bot                    the bot plays its move
//   hint                   the five best moves for the human by static equity
//   quit
// Every reply is one JSON object: {"ok":true,...state} or {"ok":false,"error":"..."}.

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "bot_factory.hpp"
#include "scrabble/board.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/move.hpp"
#include "scrabble/movegen.hpp"
#include "scrabble/rack.hpp"
#include "scrabble/tiles.hpp"

using namespace scrabble;

namespace {

std::string Quote(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

struct Turn {
  int player;
  Move move;
  int score_after;
  std::string words;  // main and cross words, for placements
};

class Session {
 public:
  Session(const Gaddag& gaddag, const Leaves& leaves, std::unique_ptr<Bot> bot)
      : gaddag_(gaddag), leaves_(leaves), bot_(std::move(bot)), gen_(gaddag), hints_(gaddag, leaves) {}

  void New(std::uint64_t seed, int seat) {
    game_ = std::make_unique<Game>(seed);
    human_ = seat;
    turns_.clear();
  }
  bool started() const { return game_ != nullptr; }

  std::string Place(const std::string& args) {
    if (game_->over()) return Error("the game is over");
    if (game_->to_move() != human_) return Error("it is not your turn");
    struct Tile_ {
      int r, c;
      char ch;
    };
    std::vector<Tile_> placed;
    std::istringstream in(args);
    std::string tok;
    while (in >> tok) {
      int r, c;
      char comma1, comma2, ch;
      std::istringstream t(tok);
      if (!(t >> r >> comma1 >> c >> comma2 >> ch) || comma1 != ',' || comma2 != ',' || r < 0 ||
          r >= kBoardDim || c < 0 || c >= kBoardDim || !std::isalpha(static_cast<unsigned char>(ch)))
        return Error("bad tile: " + tok);
      placed.push_back({r, c, ch});
    }
    if (placed.empty()) return Error("place at least one tile");
    const Board& board = game_->board();
    for (std::size_t i = 0; i < placed.size(); ++i) {
      if (!board.IsEmpty(placed[i].r, placed[i].c)) return Error("that square is taken");
      for (std::size_t j = 0; j < i; ++j)
        if (placed[i].r == placed[j].r && placed[i].c == placed[j].c)
          return Error("two tiles on one square");
    }
    // The rack must hold the tiles.
    Rack rack = game_->rack(human_);
    for (const auto& p : placed) {
      bool blank = std::islower(static_cast<unsigned char>(p.ch));
      if (!rack.Remove(blank ? kBlank : *TileFromChar(p.ch)))
        return Error("those tiles are not on your rack");
    }
    bool same_row = std::all_of(placed.begin(), placed.end(),
                                [&](const Tile_& p) { return p.r == placed[0].r; });
    bool same_col = std::all_of(placed.begin(), placed.end(),
                                [&](const Tile_& p) { return p.c == placed[0].c; });
    if (!same_row && !same_col) return Error("tiles must be in one row or one column");

    // A board with the new tiles, to read the words off.
    Board after = board;
    for (const auto& p : placed)
      after.Place(p.r, p.c, *TileFromChar(p.ch), std::islower(static_cast<unsigned char>(p.ch)));

    // Candidate moves: the main word along the row and along the column. A
    // single tile may form its main word either way.
    std::vector<Move> candidates;
    for (Direction dir : {Direction::kHorizontal, Direction::kVertical}) {
      if (dir == Direction::kHorizontal && !same_row) continue;
      if (dir == Direction::kVertical && !same_col) continue;
      const int dr = dir == Direction::kVertical ? 1 : 0, dc = 1 - dr;
      int r = placed[0].r, c = placed[0].c;
      while (r - dr >= 0 && c - dc >= 0 && !after.IsEmpty(r - dr, c - dc)) r -= dr, c -= dc;
      Move m;
      m.kind = Move::Kind::kPlace;
      m.row = r;
      m.col = c;
      m.dir = dir;
      while (r < kBoardDim && c < kBoardDim && !after.IsEmpty(r, c)) {
        if (!board.IsEmpty(r, c)) {
          m.tiles += '.';
        } else {
          char ch = 0;
          for (const auto& p : placed)
            if (p.r == r && p.c == c) ch = p.ch;
          m.tiles += ch;
        }
        r += dr, c += dc;
      }
      int news = static_cast<int>(std::count_if(m.tiles.begin(), m.tiles.end(),
                                                [](char ch) { return ch != '.'; }));
      if (news != static_cast<int>(placed.size())) {
        if (placed.size() > 1) return Error("there is a gap between your tiles");
        continue;
      }
      candidates.push_back(m);
    }
    if (candidates.empty()) return Error("there is a gap between your tiles");

    // Words that are not in the lexicon, for a clear message.
    std::vector<std::string> words = WordsFormed(after, placed);
    std::vector<std::string> bad;
    for (const auto& w : words)
      if (!gaddag_.Contains(w)) bad.push_back(w);
    if (!bad.empty()) {
      std::string msg = "not in the lexicon:";
      for (const auto& w : bad) msg += " " + w;
      return Error(msg);
    }

    std::vector<Move> legal = gen_.Generate(board, game_->rack(human_));
    // On an empty board movegen reports horizontal openings only.
    for (const Move& cand : candidates) {
      for (const Move& m : legal) {
        bool same = m.row == cand.row && m.col == cand.col && m.dir == cand.dir &&
                    m.tiles == cand.tiles;
        bool transposed = m.row == cand.col && m.col == cand.row && m.dir != cand.dir &&
                          m.tiles == cand.tiles && IsEmptyBoard(board);
        if (same || transposed) {
          Move play = cand;
          play.score = ScoreMove(board, play);
          std::string joined;
          for (const auto& w : words) joined += (joined.empty() ? "" : " ") + w;
          return Apply(play, joined);
        }
      }
    }
    if (IsEmptyBoard(board)) return Error("the first word must cover the center square");
    return Error("your word must connect to the tiles on the board");
  }

  std::string Exchange(const std::string& tiles) {
    if (game_->over()) return Error("the game is over");
    if (game_->to_move() != human_) return Error("it is not your turn");
    if (game_->bag_size() < Game::kMinBagForExchange)
      return Error("you can exchange only with 7 or more tiles in the bag");
    if (tiles.empty()) return Error("pick the tiles to exchange");
    Rack rack = game_->rack(human_);
    for (char ch : tiles) {
      auto t = TileFromChar(ch);
      if (!t || !rack.Remove(*t)) return Error("those tiles are not on your rack");
    }
    Move m;
    m.kind = Move::Kind::kExchange;
    m.tiles = tiles;
    return Apply(m, "");
  }

  std::string Pass() {
    if (game_->over()) return Error("the game is over");
    if (game_->to_move() != human_) return Error("it is not your turn");
    return Apply(Move{}, "");
  }

  std::string BotMove() {
    if (game_->over()) return Error("the game is over");
    if (game_->to_move() == human_) return Error("it is your turn");
    Move m = bot_->Choose(*game_);
    std::string joined;
    if (m.kind == Move::Kind::kPlace) {
      Board after = game_->board();
      std::vector<Placed> placed;
      const int dr = m.dir == Direction::kVertical ? 1 : 0, dc = 1 - dr;
      for (std::size_t k = 0; k < m.tiles.size(); ++k) {
        char ch = m.tiles[k];
        if (ch == '.') continue;
        int r = m.row + dr * static_cast<int>(k), c = m.col + dc * static_cast<int>(k);
        after.Place(r, c, *TileFromChar(ch), std::islower(static_cast<unsigned char>(ch)));
        placed.push_back({r, c, ch});
      }
      for (const auto& w : WordsFormed(after, placed)) joined += (joined.empty() ? "" : " ") + w;
      m.score = ScoreMove(game_->board(), m);
    }
    return Apply(m, joined);
  }

  std::string Hint() {
    if (game_->over()) return Error("the game is over");
    if (game_->to_move() != human_) return Error("it is not your turn");
    std::vector<RankedMove> ranked = hints_.Rank(*game_);
    std::string out = "{\"ok\":true,\"hints\":[";
    for (std::size_t i = 0; i < ranked.size() && i < 5; ++i) {
      if (i) out += ",";
      out += MoveJson(ranked[i].move);
      out.pop_back();
      std::ostringstream eq;
      eq.precision(1);
      eq << std::fixed << ranked[i].equity;
      out += ",\"equity\":" + eq.str() + "}";
    }
    return out + "]}";
  }

  std::string State() const {
    std::ostringstream out;
    out << "{\"ok\":true,\"human\":" << human_ << ",\"to_move\":" << game_->to_move()
        << ",\"over\":" << (game_->over() ? "true" : "false") << ",\"bag\":" << game_->bag_size()
        << ",\"scores\":[" << game_->score(0) << "," << game_->score(1) << "]"
        << ",\"rack\":" << Quote(game_->rack(human_).ToString())
        << ",\"bot_rack_size\":" << game_->rack(1 - human_).Size();
    if (game_->over()) out << ",\"bot_rack\":" << Quote(game_->rack(1 - human_).ToString());
    out << ",\"unseen\":" << Quote(UnseenTiles(game_->board(), game_->rack(human_)).ToString());
    out << ",\"board\":[";
    for (int r = 0; r < kBoardDim; ++r) {
      std::string row;
      for (int c = 0; c < kBoardDim; ++c) {
        std::uint8_t v = game_->board().At(r, c);
        if (v == Board::kEmpty) {
          row += '.';
        } else {
          char ch = TileToChar(v & ~Board::kBlankBit);
          row += (v & Board::kBlankBit) ? static_cast<char>(std::tolower(ch)) : ch;
        }
      }
      out << (r ? "," : "") << Quote(row);
    }
    out << "],\"history\":[";
    for (std::size_t i = 0; i < turns_.size(); ++i) {
      const Turn& t = turns_[i];
      std::string mj = MoveJson(t.move);
      mj.pop_back();
      out << (i ? "," : "") << mj << ",\"player\":" << t.player
          << ",\"total\":" << t.score_after << ",\"words\":" << Quote(t.words) << "}";
    }
    out << "]}";
    return out.str();
  }

 private:
  struct Placed {
    int r, c;
    char ch;
  };

  static bool IsEmptyBoard(const Board& b) {
    for (int r = 0; r < kBoardDim; ++r)
      for (int c = 0; c < kBoardDim; ++c)
        if (!b.IsEmpty(r, c)) return false;
    return true;
  }

  template <typename P>
  static std::vector<std::string> WordsFormed(const Board& after, const std::vector<P>& placed) {
    std::vector<std::string> words;
    auto word_at = [&](int r, int c, int dr, int dc) {
      while (r - dr >= 0 && c - dc >= 0 && !after.IsEmpty(r - dr, c - dc)) r -= dr, c -= dc;
      std::string w;
      int r0 = r, c0 = c;
      while (r < kBoardDim && c < kBoardDim && !after.IsEmpty(r, c)) {
        w += TileToChar(after.At(r, c) & ~Board::kBlankBit);
        r += dr, c += dc;
      }
      return std::make_pair(w, r0 * 100 + c0 + (dr ? 10000 : 0));
    };
    std::vector<int> seen;
    for (const auto& p : placed) {
      for (auto [dr, dc] : {std::pair{0, 1}, std::pair{1, 0}}) {
        auto [w, key] = word_at(p.r, p.c, dr, dc);
        if (w.size() < 2 || std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
        seen.push_back(key);
        words.push_back(w);
      }
    }
    return words;
  }

  static std::string MoveJson(const Move& m) {
    std::ostringstream out;
    const char* kind = m.kind == Move::Kind::kPlace      ? "place"
                       : m.kind == Move::Kind::kExchange ? "exchange"
                                                         : "pass";
    out << "{\"kind\":\"" << kind << "\",\"row\":" << m.row << ",\"col\":" << m.col
        << ",\"dir\":\"" << (m.dir == Direction::kHorizontal ? "H" : "V")
        << "\",\"tiles\":" << Quote(m.tiles) << ",\"score\":" << m.score
        << ",\"text\":" << Quote(m.ToString()) << "}";
    return out.str();
  }

  std::string Apply(Move m, const std::string& words) {
    int player = game_->to_move();
    int before = game_->score(player);
    if (m.kind != Move::Kind::kPlace) m.score = 0;
    game_->Apply(m);
    (void)before;
    turns_.push_back({player, m, game_->score(player), words});
    return State();
  }

  static std::string Error(const std::string& msg) {
    return "{\"ok\":false,\"error\":" + Quote(msg) + "}";
  }

  const Gaddag& gaddag_;
  const Leaves& leaves_;
  std::unique_ptr<Bot> bot_;
  MoveGenerator gen_;
  EquityBot hints_;
  std::unique_ptr<Game> game_;
  int human_ = 0;
  std::vector<Turn> turns_;
};

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, bot_kind = "equity";
  double sim_time = 3;
  int threads = 1;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << a << " needs a value\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--lexicon") lexicon = value();
    else if (a == "--leaves") leaves_path = value();
    else if (a == "--bot") bot_kind = value();
    else if (a == "--sim-time") sim_time = std::stod(value());
    else if (a == "--threads") threads = std::stoi(value());
    else {
      std::cerr << "unknown option " << a << "\n";
      return 2;
    }
  }
  if (lexicon.empty() || leaves_path.empty()) {
    std::cerr << "usage: kingbble_play --lexicon WORDS.txt --leaves LEAVES.klv2 [--bot equity|sim]\n";
    return 2;
  }
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  SimParams sim;
  sim.threads = threads;
  sim.time_limit_s = sim_time;
  Session session(gaddag, leaves, MakeBot(bot_kind, gaddag, &leaves, sim, sim_time, PegOptions{}));
  std::cout << "{\"ok\":true,\"ready\":true,\"bot\":" << Quote(bot_kind) << "}" << std::endl;

  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    std::string rest;
    std::getline(in, rest);
    rest.erase(0, rest.find_first_not_of(' '));
    std::string reply;
    try {
      if (cmd == "quit") break;
      if (cmd == "new") {
        std::uint64_t seed = 1;
        int seat = 0;
        std::istringstream a(rest);
        a >> seed >> seat;
        session.New(seed, seat == 1 ? 1 : 0);
        reply = session.State();
      } else if (!session.started()) {
        reply = "{\"ok\":false,\"error\":\"no game: send new first\"}";
      } else if (cmd == "state") {
        reply = session.State();
      } else if (cmd == "place") {
        reply = session.Place(rest);
      } else if (cmd == "exchange") {
        reply = session.Exchange(rest);
      } else if (cmd == "pass") {
        reply = session.Pass();
      } else if (cmd == "bot") {
        reply = session.BotMove();
      } else if (cmd == "hint") {
        reply = session.Hint();
      } else {
        reply = "{\"ok\":false,\"error\":" + Quote("unknown command: " + cmd) + "}";
      }
    } catch (const std::exception& e) {
      reply = "{\"ok\":false,\"error\":" + Quote(e.what()) + "}";
    }
    std::cout << reply << std::endl;
  }
  return 0;
}
