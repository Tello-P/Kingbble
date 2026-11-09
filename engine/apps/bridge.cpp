// Bridge between the engine and any interface (the web UI in ui/, a desktop app,
// tests): one game, each seat a human or a bot, driven by one text command per
// line on stdin, answered by one JSON object per line on stdout.
//
//   kingbble_bridge --lexicon lexica/CSW24.txt [--leaves lexica/CSW24.klv2]
//                   [--human-params models/human_model.txt] [--threads 2]
//
// Commands (racks of bots facing a human stay hidden until the game ends):
//   new SEED P0 P1 S0 S1        new game; P0 moves first; each P is "human" or a bot
//                               (bot_factory.hpp names), S its seconds per move
//   new SEED SEAT BOT SECONDS   shorthand: a human in SEAT against BOT
//   state                       current state
//   play R,C,L R,C,L ...        place tiles (0-based row and column; lowercase L = blank)
//   check R,C,L ...             same as play, but only returns the move and its score
//   exchange TILES              exchange ('?' for a blank)
//   pass
//   bot                         the bot to move plays
//   hint N                      the N best moves of the player to move by static equity
//                               (by score without leaves)
//   undo                        takes back the last human move and the bot moves after it
//                               (the last move if no seat is human)
//   gcg                         the game so far as GCG
//   quit
// Every answer has "ok"; errors carry "error", everything else carries "state".

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bot_factory.hpp"
#include "game_record.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/game.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/inference.hpp"
#include "scrabble/kwg.hpp"
#include "scrabble/movegen.hpp"

using namespace scrabble;

namespace {

std::string Esc(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (static_cast<unsigned char>(c) >= 0x20) {
      out += c;
    }
  }
  return out;
}

std::string Str(const std::string& s) { return "\"" + Esc(s) + "\""; }

// Squares of the new tiles of a placement, with their characters.
std::set<std::pair<std::pair<int, int>, char>> NewTiles(const Move& m) {
  std::set<std::pair<std::pair<int, int>, char>> out;
  const int dr = m.dir == Direction::kVertical ? 1 : 0, dc = 1 - dr;
  for (std::size_t k = 0; k < m.tiles.size(); ++k)
    if (m.tiles[k] != '.')
      out.insert(
          {{m.row + dr * static_cast<int>(k), m.col + dc * static_cast<int>(k)}, m.tiles[k]});
  return out;
}

char BonusChar(Bonus b) {
  switch (b) {
    case Bonus::kDoubleLetter:
      return 'd';
    case Bonus::kTripleLetter:
      return 't';
    case Bonus::kDoubleWord:
      return 'D';
    case Bonus::kTripleWord:
      return 'T';
    case Bonus::kNone:
      break;
  }
  return '.';
}

struct Played {
  int player;
  Move move;
  std::string rack;  // the mover's rack before the move
  int cumulative;
};

class Session {
 public:
  Session(const Gaddag& gaddag, const Leaves* leaves, const HumanBotOptions& human, int threads)
      : gaddag_(gaddag), leaves_(leaves), human_(human), threads_(threads), gen_(gaddag) {
    gen_.set_vertical_openings(true);
  }

  std::string New(std::uint64_t seed, const std::array<std::string, 2>& kinds,
                  const std::array<double, 2>& seconds) {
    for (const std::string& kind : kinds) {
      if (kind == "human") continue;
      if (kind != "greedy" && !leaves_) return Error("bot '" + kind + "' needs --leaves");
      if (kind.find("human") != std::string::npos && !human_.params)
        return Error("bot '" + kind + "' needs --human-params");
    }
    seed_ = seed;
    kinds_ = kinds;
    seconds_ = seconds;
    for (int p = 0; p < 2; ++p) {
      setups_[p] = BotSetup{};
      if (IsHuman(p)) continue;
      SimParams sim;
      sim.threads = threads_;
      sim.seed = seed + 0x9e37 * p;
      sim.time_limit_s = seconds[p];
      PegOptions peg;
      setups_[p] = MakeBotSetup(kinds[p], gaddag_, leaves_, sim, seconds[p] > 0 ? seconds[p] : 5,
                                peg, {}, human_);
    }
    history_.clear();
    Replay();
    return Ok();
  }

  // With `apply` false, only checks the placement and returns its score.
  std::string Play(const std::vector<std::string>& args, bool apply = true) {
    if (auto e = HumanTurn()) return *e;
    std::set<std::pair<std::pair<int, int>, char>> want;
    for (const std::string& a : args) {
      int r, c;
      char l;
      if (std::sscanf(a.c_str(), "%d,%d,%c", &r, &c, &l) != 3 || r < 0 || r >= kBoardDim || c < 0 ||
          c >= kBoardDim || !std::isalpha(static_cast<unsigned char>(l)))
        return Error("bad tile: " + a);
      if (!want.insert({{r, c}, l}).second) return Error("square used twice");
    }
    if (want.empty()) return Error("no tiles");
    for (const Move& m : gen_.Generate(game_->board(), game_->rack(game_->to_move())))
      if (NewTiles(m) == want) {
        if (apply) return Apply(m);
        return "{\"ok\":true,\"score\":" + std::to_string(ScoreMove(game_->board(), m)) +
               ",\"move\":" + MoveJson(m) + "}";
      }
    return Error(Diagnose(want));
  }

  std::string Exchange(const std::string& tiles) {
    if (auto e = HumanTurn()) return *e;
    if (game_->bag_size() < Game::kMinBagForExchange)
      return Error("the bag needs at least 7 tiles to exchange");
    Rack rack = game_->rack(game_->to_move());
    Move m;
    m.kind = Move::Kind::kExchange;
    for (char ch : tiles) {
      auto t = TileFromChar(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
      if (!t || !rack.Remove(*t)) return Error(std::string("not on your rack: ") + ch);
      m.tiles += TileToChar(*t);
    }
    if (m.tiles.empty()) return Error("no tiles");
    return Apply(m);
  }

  std::string Pass() {
    if (auto e = HumanTurn()) return *e;
    return Apply(Move{});
  }

  std::string BotMove() {
    if (!game_) return Error("no game");
    if (game_->over()) return Error("the game is over");
    if (IsHuman(game_->to_move())) return Error("it is your turn");
    Move m = setups_[game_->to_move()].bot->Choose(*game_);
    return Apply(m);
  }

  std::string Hint(int n) {
    if (!game_) return Error("no game");
    const Rack& rack = game_->rack(game_->to_move());
    std::vector<Move> moves = gen_.Generate(game_->board(), rack);
    if (game_->bag_size() >= Game::kMinBagForExchange)
      for (Move& m : GenerateExchanges(rack)) moves.push_back(std::move(m));
    moves.push_back(Move{});
    const Rack unseen = UnseenTiles(game_->board(), rack);
    std::vector<std::pair<double, Move>> scored;
    for (Move& m : moves) {
      if (m.kind == Move::Kind::kPlace) m.score = ScoreMove(game_->board(), m);
      double eq = leaves_ ? StaticEquity(m, rack, game_->bag_size(), unseen, *leaves_) : m.score;
      scored.push_back({eq, std::move(m)});
    }
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
      return a.first != b.first ? a.first > b.first : TiebreaksBetter(a.second, b.second);
    });
    std::ostringstream o;
    o << "{\"ok\":true,\"hints\":[";
    for (int i = 0; i < n && i < static_cast<int>(scored.size()); ++i) {
      if (i) o << ",";
      o << "{\"equity\":" << scored[i].first << ",\"move\":" << MoveJson(scored[i].second) << "}";
    }
    o << "]}";
    return o.str();
  }

  std::string Undo() {
    if (!game_) return Error("no game");
    std::size_t keep = history_.size();
    if (IsHuman(0) || IsHuman(1)) {
      while (keep > 0 && !IsHuman(history_[keep - 1].player)) --keep;
    }
    if (keep == 0) return Error("nothing to undo");
    history_.resize(keep - 1);
    Replay();
    return Ok();
  }

  std::string Gcg() {
    if (!game_) return Error("no game");
    std::array<std::string, 2> nicks, names;
    for (int p = 0; p < 2; ++p) {
      nicks[p] = (IsHuman(p) ? "player" : "kingbble") + std::to_string(p + 1);
      names[p] = kinds_[p];
    }
    GameRecord record(nicks, names, "", "kingbble-ui-" + std::to_string(seed_));
    Game g(seed_);
    for (const Played& p : history_) {
      record.Add(g, p.move);
      g.Apply(p.move);
    }
    if (g.over()) record.Finish(g);
    return "{\"ok\":true,\"gcg\":" + Str(WriteGcg(record.gcg())) + "}";
  }

  std::string Ok() {
    if (!game_) return Error("no game");
    return "{\"ok\":true,\"state\":" + State() + "}";
  }

  static std::string Error(const std::string& msg) {
    return "{\"ok\":false,\"error\":" + Str(msg) + "}";
  }

 private:
  bool IsHuman(int p) const { return kinds_[p] == "human"; }

  // Racks a viewer may see: all of them with no human seat or once the game is
  // over, otherwise only the humans'.
  bool Visible(int p) const {
    return (!IsHuman(0) && !IsHuman(1)) || IsHuman(p) || (game_ && game_->over());
  }

  std::optional<std::string> HumanTurn() {
    if (!game_) return Error("no game");
    if (game_->over()) return Error("the game is over");
    if (!IsHuman(game_->to_move())) return Error("it is the bot's turn");
    return std::nullopt;
  }

  // Why no legal move places exactly these tiles.
  std::string Diagnose(const std::set<std::pair<std::pair<int, int>, char>>& want) {
    const Board& b = game_->board();
    Rack rack = game_->rack(game_->to_move());
    std::set<int> rows, cols;
    for (const auto& [sq, l] : want) {
      if (!b.IsEmpty(sq.first, sq.second)) return "square already taken";
      bool blank = std::islower(static_cast<unsigned char>(l));
      Tile t = *TileFromChar(static_cast<char>(std::toupper(static_cast<unsigned char>(l))));
      if (!rack.Remove(blank ? kBlank : t)) return std::string("not on your rack: ") + l;
      rows.insert(sq.first);
      cols.insert(sq.second);
    }
    if (rows.size() > 1 && cols.size() > 1) return "tiles must be in one row or column";
    if (b.IsEmpty(kCenter, kCenter)) {
      bool center = false;
      for (const auto& [sq, l] : want) center |= sq == std::pair{kCenter, kCenter};
      if (!center) return "the first move must cover the centre square";
    }
    return "not a valid move (gaps, unconnected or a word not in the lexicon)";
  }

  std::string Apply(Move m) {
    if (m.kind == Move::Kind::kPlace) m.score = ScoreMove(game_->board(), m);
    const int player = game_->to_move();
    const std::string rack = game_->rack(player).ToString();
    for (BotSetup& s : setups_) s.BeforeMove(*game_, m);
    game_->Apply(m);
    for (BotSetup& s : setups_) s.AfterMove(*game_);
    history_.push_back({player, m, rack, game_->score(player)});
    return Ok();
  }

  // Rebuilds the game (and the bots' trackers) from the seed and the moves played.
  void Replay() {
    std::vector<Played> moves = std::move(history_);
    history_.clear();
    game_.emplace(seed_);
    for (int p = 0; p < 2; ++p)
      if (!IsHuman(p)) setups_[p].NewGame(*game_, p, seed_ + 0x9e37 * p);
    for (const Played& p : moves) Apply(p.move);
  }

  static std::string MoveJson(const Move& m) {
    std::ostringstream o;
    const char* kind = m.kind == Move::Kind::kPlace      ? "place"
                       : m.kind == Move::Kind::kExchange ? "exchange"
                                                         : "pass";
    o << "{\"kind\":\"" << kind << "\",\"text\":" << Str(m.ToString()) << ",\"score\":" << m.score;
    if (m.kind == Move::Kind::kPlace) {
      o << ",\"row\":" << m.row << ",\"col\":" << m.col << ",\"dir\":\""
        << (m.dir == Direction::kHorizontal ? "h" : "v") << "\",\"tiles\":" << Str(m.tiles)
        << ",\"placed\":[";
      bool first = true;
      for (const auto& [sq, l] : NewTiles(m)) {
        o << (first ? "" : ",") << "[" << sq.first << "," << sq.second << ",\"" << l << "\"]";
        first = false;
      }
      o << "]";
    } else if (m.kind == Move::Kind::kExchange) {
      o << ",\"count\":" << m.tiles.size() << ",\"exchanged\":" << Str(m.tiles);
    }
    o << "}";
    return o.str();
  }

  std::string State() {
    const Game& g = *game_;
    std::ostringstream o;
    o << "{\"players\":[" << Str(kinds_[0]) << "," << Str(kinds_[1]) << "],\"seconds\":["
      << seconds_[0] << "," << seconds_[1] << "],\"seed\":" << seed_
      << ",\"to_move\":" << g.to_move() << ",\"over\":" << (g.over() ? "true" : "false")
      << ",\"bag\":" << g.bag_size() << ",\"scores\":[" << g.score(0) << "," << g.score(1)
      << "],\"racks\":[";
    for (int p = 0; p < 2; ++p)
      o << (p ? "," : "") << (Visible(p) ? Str(g.rack(p).ToString()) : "null");
    o << "],\"board\":[";
    for (int r = 0; r < kBoardDim; ++r) {
      std::string row;
      for (int c = 0; c < kBoardDim; ++c) {
        if (g.board().IsEmpty(r, c)) {
          row += '.';
          continue;
        }
        std::uint8_t v = g.board().At(r, c);
        char ch = TileToChar(static_cast<Tile>(v & ~Board::kBlankBit));
        row += (v & Board::kBlankBit) ? static_cast<char>(std::tolower(ch)) : ch;
      }
      o << (r ? "," : "") << Str(row);
    }
    o << "],\"bonus\":[";
    for (int r = 0; r < kBoardDim; ++r) {
      std::string row;
      for (int c = 0; c < kBoardDim; ++c) row += BonusChar(Board::BonusAt(r, c));
      o << (r ? "," : "") << Str(row);
    }
    // Tiles the player to move (or the human) cannot see: the bag plus the other rack.
    int viewer = g.to_move();
    if (!IsHuman(viewer) && IsHuman(1 - viewer)) viewer = 1 - viewer;
    o << "],\"viewer\":" << viewer << ",\"unseen\":{";
    const Rack unseen = UnseenTiles(g.board(), g.rack(viewer));
    bool first = true;
    for (int t = 0; t < kNumTileTypes; ++t) {
      if (!unseen.Count(Tile(t))) continue;
      o << (first ? "" : ",") << "\"" << TileToChar(Tile(t)) << "\":" << unseen.Count(Tile(t));
      first = false;
    }
    o << "},\"values\":{";
    for (int t = 0; t < kNumTileTypes; ++t)
      o << (t ? "," : "") << "\"" << TileToChar(Tile(t)) << "\":" << int(EnglishTiles()[t].score);
    o << "},\"history\":[";
    for (std::size_t i = 0; i < history_.size(); ++i) {
      const Played& p = history_[i];
      o << (i ? "," : "") << "{\"player\":" << p.player << ",\"cumulative\":" << p.cumulative
        << ",\"move\":" << MoveJson(p.move);
      if (Visible(p.player)) o << ",\"rack\":" << Str(p.rack);
      o << "}";
    }
    o << "]}";
    return o.str();
  }

  const Gaddag& gaddag_;
  const Leaves* leaves_;
  HumanBotOptions human_;
  int threads_;
  MoveGenerator gen_;
  std::optional<Game> game_;
  std::array<BotSetup, 2> setups_;
  std::vector<Played> history_;
  std::uint64_t seed_ = 1;
  std::array<std::string, 2> kinds_{"human", "human"};
  std::array<double, 2> seconds_{};
};

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, human_path;
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
    if (a == "--lexicon") {
      lexicon = value();
    } else if (a == "--leaves") {
      leaves_path = value();
    } else if (a == "--human-params") {
      human_path = value();
    } else if (a == "--threads") {
      threads = std::stoi(value());
    } else {
      std::cerr << "usage: kingbble_bridge --lexicon FILE [--leaves KLV] [--human-params FILE]"
                   " [--threads N]\n";
      return 2;
    }
  }
  if (lexicon.empty()) {
    std::cerr << "--lexicon is required\n";
    return 2;
  }
  const Gaddag gaddag =
      lexicon.ends_with(".kwg") ? Kwg::FromFile(lexicon).ToGaddag() : Gaddag::FromFile(lexicon);
  std::unique_ptr<Leaves> leaves;
  if (!leaves_path.empty()) leaves = std::make_unique<Leaves>(Leaves::FromFile(leaves_path));
  HumanModelParams human_params;
  HumanBotOptions human;
  if (!human_path.empty()) {
    human_params = HumanModelParams::FromFile(human_path);
    human.params = &human_params;
  }

  Session session(gaddag, leaves.get(), human, threads);
  std::cout << "{\"ok\":true,\"ready\":true,\"words\":" << gaddag.NumWords()
            << ",\"leaves\":" << (leaves ? "true" : "false") << "}" << std::endl;
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    std::vector<std::string> args;
    for (std::string a; in >> a;) args.push_back(a);
    std::string out;
    try {
      if (cmd == "new" && args.size() == 5) {
        out = session.New(std::stoull(args[0]), {args[1], args[2]},
                          {std::stod(args[3]), std::stod(args[4])});
      } else if (cmd == "new" && args.size() == 4) {
        const int seat = std::stoi(args[1]);
        if (seat != 0 && seat != 1) {
          out = Session::Error("seat must be 0 or 1");
        } else {
          std::array<std::string, 2> kinds{args[2], args[2]};
          kinds[seat] = "human";
          const double s = std::stod(args[3]);
          out = session.New(std::stoull(args[0]), kinds, {s, s});
        }
      } else if (cmd == "state") {
        out = session.Ok();
      } else if (cmd == "play") {
        out = session.Play(args);
      } else if (cmd == "check") {
        out = session.Play(args, false);
      } else if (cmd == "exchange" && args.size() == 1) {
        out = session.Exchange(args[0]);
      } else if (cmd == "pass") {
        out = session.Pass();
      } else if (cmd == "bot") {
        out = session.BotMove();
      } else if (cmd == "hint") {
        out = session.Hint(args.empty() ? 10 : std::stoi(args[0]));
      } else if (cmd == "undo") {
        out = session.Undo();
      } else if (cmd == "gcg") {
        out = session.Gcg();
      } else if (cmd == "quit") {
        break;
      } else if (cmd.empty()) {
        continue;
      } else {
        out = Session::Error("unknown command: " + line);
      }
    } catch (const std::exception& e) {
      out = Session::Error(e.what());
    }
    std::cout << out << std::endl;
  }
  return 0;
}
