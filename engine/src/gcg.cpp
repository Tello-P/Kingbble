#include "scrabble/gcg.hpp"

#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <utility>

#include "scrabble/movegen.hpp"

namespace scrabble {

namespace {

bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool IsLower(char c) { return c >= 'a' && c <= 'z'; }

std::string_view Trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}

std::vector<std::string_view> Split(std::string_view s) {
  std::vector<std::string_view> out;
  std::size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    std::size_t j = i;
    while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
    if (j > i) out.push_back(s.substr(i, j - i));
    i = j;
  }
  return out;
}

bool ParseInt(std::string_view s, int& out) {
  if (!s.empty() && s.front() == '+') s.remove_prefix(1);
  if (s.empty()) return false;
  auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
  return ec == std::errc() && p == s.data() + s.size();
}

// "8D" = row 8, column D, horizontal; "D8" = vertical.
bool ParseCoord(std::string_view s, Move& m) {
  if (s.size() < 2 || s.size() > 3) return false;
  std::string_view digits, letter;
  if (std::isdigit(static_cast<unsigned char>(s.front()))) {
    m.dir = Direction::kHorizontal;
    digits = s.substr(0, s.size() - 1);
    letter = s.substr(s.size() - 1);
  } else {
    m.dir = Direction::kVertical;
    letter = s.substr(0, 1);
    digits = s.substr(1);
  }
  int row;
  if (!ParseInt(digits, row) || digits.front() == '+') return false;
  char c = static_cast<char>(std::toupper(static_cast<unsigned char>(letter.front())));
  if (row < 1 || row > kBoardDim || c < 'A' || c >= 'A' + kBoardDim) return false;
  m.row = row - 1;
  m.col = c - 'A';
  return true;
}

// Letters as written; "(XY)" groups mark played-through squares.
bool ParseWord(std::string_view s, std::string& tiles) {
  tiles.clear();
  bool through = false;
  for (char c : s) {
    if (c == '(' && !through) {
      through = true;
    } else if (c == ')' && through) {
      through = false;
    } else if (IsUpper(c) || IsLower(c) || c == '.') {
      tiles.push_back(through ? '.' : c);
    } else {
      return false;
    }
  }
  return !through && !tiles.empty();
}

bool IsRack(std::string_view s) {
  for (char c : s)
    if (!IsUpper(c) && c != '?') return false;
  return true;
}

bool IsParenRack(std::string_view s) {
  return s.size() >= 2 && s.front() == '(' && s.back() == ')' && IsRack(s.substr(1, s.size() - 2));
}

bool ParseMoveLine(std::string_view body, GcgEvent& ev, std::string& why) {
  auto tok = Split(body);
  if (tok.size() < 3) {
    why = "too few fields";
    return false;
  }
  if (!ParseInt(tok.back(), ev.cumulative)) {
    why = "bad cumulative score";
    return false;
  }
  if (!ParseInt(tok[tok.size() - 2], ev.score)) {
    why = "bad score";
    return false;
  }
  tok.resize(tok.size() - 2);

  if (tok.size() == 1 && IsParenRack(tok[0])) {
    ev.kind = GcgEvent::Kind::kEndRackPoints;
    ev.rack = std::string(tok[0].substr(1, tok[0].size() - 2));
    return true;
  }
  if (!IsRack(tok[0])) {
    why = "bad rack";
    return false;
  }
  ev.rack = std::string(tok[0]);
  if (tok.size() == 2) {
    std::string_view a = tok[1];
    if (a == "--") {
      ev.kind = GcgEvent::Kind::kPhonyWithdrawn;
    } else if (a == "-") {
      ev.kind = GcgEvent::Kind::kPass;
      ev.move.kind = Move::Kind::kPass;
    } else if (a.front() == '-') {
      ev.kind = GcgEvent::Kind::kExchange;
      ev.move.kind = Move::Kind::kExchange;
      std::string_view rest = a.substr(1);
      if (ParseInt(rest, ev.exchanged) && rest.front() != '+') {
        if (ev.exchanged < 1 || ev.exchanged > kRackSize) {
          why = "bad exchange count";
          return false;
        }
      } else if (IsRack(rest)) {
        ev.move.tiles = std::string(rest);
        ev.exchanged = static_cast<int>(rest.size());
      } else {
        why = "bad exchange";
        return false;
      }
    } else if (a == "(challenge)") {
      ev.kind = GcgEvent::Kind::kChallengeBonus;
    } else if (a == "(time)") {
      ev.kind = GcgEvent::Kind::kTimePenalty;
    } else if (IsParenRack(a)) {
      ev.kind = GcgEvent::Kind::kEndRackPenalty;
    } else {
      why = "unknown move";
      return false;
    }
    return true;
  }
  if (tok.size() == 3) {
    ev.kind = GcgEvent::Kind::kPlace;
    ev.move.kind = Move::Kind::kPlace;
    if (!ParseCoord(tok[1], ev.move)) {
      why = "bad coordinates";
      return false;
    }
    if (!ParseWord(tok[2], ev.move.tiles)) {
      why = "bad word";
      return false;
    }
    ev.move.score = ev.score;
    return true;
  }
  why = "unexpected fields";
  return false;
}

std::string Signed(int v) { return (v >= 0 ? "+" : "") + std::to_string(v); }

std::string Coord(const Move& m) {
  std::string r = std::to_string(m.row + 1);
  std::string c(1, static_cast<char>('A' + m.col));
  return m.dir == Direction::kHorizontal ? r + c : c + r;
}

int TilesValue(std::string_view tiles) {
  int v = 0;
  for (char c : tiles)
    if (IsUpper(c)) v += EnglishTiles()[*TileFromChar(c)].score;
  return v;
}

}  // namespace

std::optional<GcgGame> ParseGcg(std::string_view text, std::string* error) {
  GcgGame game;
  int line_no = 0;
  bool explicit_players = false;
  auto fail = [&](const std::string& why) -> std::optional<GcgGame> {
    if (error) *error = "line " + std::to_string(line_no) + ": " + why;
    return std::nullopt;
  };
  while (!text.empty()) {
    std::size_t nl = text.find('\n');
    std::string_view raw = text.substr(0, nl);
    text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
    ++line_no;
    std::string_view line = Trim(raw);
    if (line.empty()) continue;

    if (line.front() == '#') {
      auto tok = Split(line);
      std::string_view key = tok[0];
      if ((key == "#player1" || key == "#player2") && tok.size() >= 2) {
        int p = key == "#player1" ? 0 : 1;
        game.nicknames[p] = std::string(tok[1]);
        std::size_t at = line.find(tok[1], key.size()) + tok[1].size();
        game.names[p] = std::string(Trim(line.substr(at)));
        explicit_players = true;
      } else if (key == "#lexicon" && tok.size() >= 2) {
        game.lexicon = std::string(tok[1]);
      } else if (key == "#id" && tok.size() >= 2) {
        game.id = std::string(Trim(line.substr(3)));
      }
      continue;  // other pragmas (#note, #title, #rack1, ...) are ignored
    }
    if (line.front() != '>') continue;  // continuation of a multi-line note

    std::size_t colon = line.find(':');
    if (colon == std::string_view::npos) return fail("missing ':'");
    std::string nick(Trim(line.substr(1, colon - 1)));
    GcgEvent ev;
    ev.line = line_no;
    if (nick == game.nicknames[0]) {
      ev.player = 0;
    } else if (nick == game.nicknames[1]) {
      ev.player = 1;
    } else if (!explicit_players && game.nicknames[1].empty()) {
      ev.player = game.nicknames[0].empty() ? 0 : 1;
      game.nicknames[ev.player] = nick;
    } else {
      return fail("unknown player '" + nick + "'");
    }
    std::string why;
    if (!ParseMoveLine(line.substr(colon + 1), ev, why)) return fail(why);
    game.events.push_back(std::move(ev));
  }
  return game;
}

std::optional<GcgGame> ReadGcgFile(const std::string& path, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) *error = "cannot open " + path;
    return std::nullopt;
  }
  std::stringstream ss;
  ss << in.rdbuf();
  return ParseGcg(ss.str(), error);
}

std::string WriteGcg(const GcgGame& game) {
  std::string out;
  for (int p = 0; p < 2; ++p) {
    out += "#player" + std::to_string(p + 1) + " " + game.nicknames[p];
    if (!game.names[p].empty()) out += " " + game.names[p];
    out += "\n";
  }
  if (!game.lexicon.empty()) out += "#lexicon " + game.lexicon + "\n";
  if (!game.id.empty()) out += "#id " + game.id + "\n";
  for (const GcgEvent& ev : game.events) {
    out += ">" + game.nicknames[ev.player] + ":";
    switch (ev.kind) {
      case GcgEvent::Kind::kPlace:
        out += " " + ev.rack + " " + Coord(ev.move) + " " + ev.move.tiles;
        break;
      case GcgEvent::Kind::kExchange:
        out += " " + ev.rack + " -" +
               (ev.move.tiles.empty() ? std::to_string(ev.exchanged) : ev.move.tiles);
        break;
      case GcgEvent::Kind::kPass:
        out += " " + ev.rack + " -";
        break;
      case GcgEvent::Kind::kPhonyWithdrawn:
        out += " " + ev.rack + " --";
        break;
      case GcgEvent::Kind::kChallengeBonus:
        out += " " + ev.rack + " (challenge)";
        break;
      case GcgEvent::Kind::kEndRackPoints:
        out += " (" + ev.rack + ")";
        break;
      case GcgEvent::Kind::kEndRackPenalty:
        out += " " + ev.rack + " (" + ev.rack + ")";
        break;
      case GcgEvent::Kind::kTimePenalty:
        out += " " + ev.rack + " (time)";
        break;
    }
    out += " " + Signed(ev.score) + " " + std::to_string(ev.cumulative) + "\n";
  }
  return out;
}

GcgReplay ReplayGcg(const GcgGame& game) {
  GcgReplay r;
  // Squares filled by the last placement, for phony withdrawals.
  std::vector<std::pair<int, int>> last_placed;
  int last_placer = -1;
  int last_score = 0;
  for (const GcgEvent& ev : game.events) {
    auto fail = [&](const std::string& why) {
      r.error = "line " + std::to_string(ev.line) + ": " + why;
      return r;
    };
    r.boards_before.push_back(r.board);
    Move played = ev.move;
    switch (ev.kind) {
      case GcgEvent::Kind::kPlace: {
        const int dr = played.dir == Direction::kVertical ? 1 : 0, dc = 1 - dr;
        const int n = static_cast<int>(played.tiles.size());
        if (played.row + dr * (n - 1) >= kBoardDim || played.col + dc * (n - 1) >= kBoardDim)
          return fail("word does not fit on the board");
        Rack rack;
        const bool check_rack = !ev.rack.empty();
        if (check_rack) rack = *Rack::FromString(ev.rack);
        for (int k = 0; k < n; ++k) {
          const int row = played.row + dr * k, col = played.col + dc * k;
          char& ch = played.tiles[k];
          if (!r.board.IsEmpty(row, col)) {
            char on_board = TileToChar(r.board.At(row, col) & ~Board::kBlankBit);
            if (ch != '.' && std::toupper(static_cast<unsigned char>(ch)) != on_board)
              return fail("word does not match the board");
            ch = '.';
          } else if (ch == '.') {
            return fail("played-through square is empty");
          } else if (check_rack && !rack.Remove(IsLower(ch) ? kBlank : *TileFromChar(ch))) {
            return fail("tile not on rack");
          }
        }
        if (played.TilesPlayed() == 0) return fail("no tiles placed");
        int expected = ScoreMove(r.board, played);
        if (expected != ev.score)
          return fail("score " + std::to_string(ev.score) + ", expected " +
                      std::to_string(expected));
        last_placed.clear();
        for (int k = 0; k < n; ++k) {
          if (played.tiles[k] == '.') continue;
          const int row = played.row + dr * k, col = played.col + dc * k;
          r.board.Place(row, col, *TileFromChar(played.tiles[k]), IsLower(played.tiles[k]));
          last_placed.emplace_back(row, col);
        }
        last_placer = ev.player;
        last_score = ev.score;
        played.score = ev.score;
        break;
      }
      case GcgEvent::Kind::kExchange:
      case GcgEvent::Kind::kPass:
        if (ev.score != 0) return fail("exchange or pass with a nonzero score");
        break;
      case GcgEvent::Kind::kPhonyWithdrawn:
        if (last_placer != ev.player || last_placed.empty())
          return fail("withdrawal without a previous placement");
        if (ev.score != -last_score) return fail("withdrawal does not cancel the placement");
        for (auto [row, col] : last_placed) r.board.Clear(row, col);
        last_placed.clear();
        last_placer = -1;
        break;
      case GcgEvent::Kind::kEndRackPoints:
        if (ev.score != 2 * TilesValue(ev.rack)) return fail("end rack points mismatch");
        break;
      case GcgEvent::Kind::kEndRackPenalty:
        if (ev.score != -TilesValue(ev.rack)) return fail("end rack penalty mismatch");
        break;
      case GcgEvent::Kind::kChallengeBonus:
      case GcgEvent::Kind::kTimePenalty:
        break;
    }
    if (ev.kind != GcgEvent::Kind::kPlace) last_placed.clear();
    r.scores[ev.player] += ev.score;
    if (r.scores[ev.player] != ev.cumulative)
      return fail("cumulative " + std::to_string(ev.cumulative) + ", expected " +
                  std::to_string(r.scores[ev.player]));
    r.moves.push_back(played);
  }
  r.ok = true;
  return r;
}

}  // namespace scrabble
