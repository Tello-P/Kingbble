#include "scrabble/value_model.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "scrabble/board.hpp"
#include "scrabble/equity.hpp"

namespace scrabble {

namespace {

bool IsVowel(Tile t) { return t == 1 || t == 5 || t == 9 || t == 15 || t == 21; }

constexpr Tile kS = 19, kQ = 17, kU = 21;

}  // namespace

std::array<double, kValueFeatures> ValueFeatures(const Game& game, const Leaves& leaves) {
  const int me = game.to_move();
  const Rack& rack = game.rack(me);
  const Rack unseen = UnseenTiles(game.board(), rack);
  std::array<double, kValueFeatures> f{};
  f[0] = game.score(me) - game.score(1 - me);
  f[1] = unseen.Size();
  f[2] = game.bag_size();
  f[3] = game.scoreless_turns();
  f[4] = rack.Size();
  f[5] = rack.Size() < kRackSize ? leaves.Value(rack) : 0;
  int rack_points = 0, unseen_points = 0, unseen_vowels = 0;
  for (int t = 0; t < kNumTileTypes; ++t) {
    const Tile tile = Tile(t);
    if (IsVowel(tile)) f[6] += rack.Count(tile), unseen_vowels += unseen.Count(tile);
    rack_points += rack.Count(tile) * EnglishTiles()[t].score;
    unseen_points += unseen.Count(tile) * EnglishTiles()[t].score;
  }
  f[7] = rack.Count(kBlank);
  f[8] = rack.Count(kS);
  f[9] = rack.Count(kQ) > 0 && rack.Count(kU) == 0;
  f[10] = unseen.Count(kBlank);
  f[11] = unseen.Count(kS);
  f[12] = unseen.Size() ? double(unseen_vowels) / unseen.Size() : 0;
  // Premium word squares still open: empty, with a tile somewhere in their row or column.
  std::array<bool, kBoardDim> row_used{}, col_used{};
  int tiles = 0;
  for (int r = 0; r < kBoardDim; ++r)
    for (int c = 0; c < kBoardDim; ++c)
      if (!game.board().IsEmpty(r, c)) row_used[r] = col_used[c] = true, ++tiles;
  f[13] = tiles;
  for (int r = 0; r < kBoardDim; ++r) {
    for (int c = 0; c < kBoardDim; ++c) {
      if (!game.board().IsEmpty(r, c) || !(row_used[r] || col_used[c])) continue;
      const Bonus b = Board::BonusAt(r, c);
      f[14] += b == Bonus::kTripleWord;
      f[15] += b == Bonus::kDoubleWord;
    }
  }
  f[16] = unseen_points;
  f[17] = rack_points;
  // Anchors (empty squares next to a tile), open premium word squares among them, and
  // bingo lanes: lines with 7 empty squares in a row, one of them an anchor.
  const Board& board = game.board();
  auto empty = [&](int r, int c) {
    return r < 0 || r >= kBoardDim || c < 0 || c >= kBoardDim || board.IsEmpty(r, c);
  };
  auto anchor = [&](int r, int c) {
    return board.IsEmpty(r, c) &&
           (!empty(r - 1, c) || !empty(r + 1, c) || !empty(r, c - 1) || !empty(r, c + 1));
  };
  for (int r = 0; r < kBoardDim; ++r) {
    for (int c = 0; c < kBoardDim; ++c) {
      if (!anchor(r, c)) continue;
      ++f[19];
      const Bonus b = Board::BonusAt(r, c);
      f[20] += b == Bonus::kTripleWord;
      f[21] += b == Bonus::kDoubleWord;
    }
  }
  for (int line = 0; line < kBoardDim; ++line) {
    for (int dir = 0; dir < 2; ++dir) {
      int run = 0;
      bool touches = false, counted = false;
      for (int i = 0; i < kBoardDim && !counted; ++i) {
        const int r = dir ? i : line, c = dir ? line : i;
        if (!board.IsEmpty(r, c)) {
          run = 0;
          touches = false;
          continue;
        }
        ++run;
        touches = touches || anchor(r, c);
        if (run >= kRackSize && touches) counted = true;
      }
      f[18] += counted;
    }
  }
  int duplicates = 0, rack_heavy = 0, unseen_heavy = 0;
  for (int t = 1; t < kNumTileTypes; ++t) {
    duplicates += std::max(0, rack.Count(Tile(t)) - 1);
    if (EnglishTiles()[t].score >= 8)
      rack_heavy += rack.Count(Tile(t)), unseen_heavy += unseen.Count(Tile(t));
  }
  f[22] = rack.Size() - f[6] - f[7];  // consonants
  f[23] = duplicates;
  f[24] = rack_heavy;
  f[25] = unseen_heavy;
  return f;
}

ValueModel ValueModel::FromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::stringstream text;
  text << in.rdbuf();
  return FromText(text.str());
}

ValueModel ValueModel::FromText(const std::string& text) {
  std::istringstream in(text);
  std::string magic;
  int version = 0, features = 0;
  if (!(in >> magic >> version >> features) || magic != "kingbble-value" || version != 1 ||
      features != kValueFeatures)
    throw std::runtime_error("not a value model for these features");
  ValueModel model;
  std::string word;
  while (in >> word) {
    if (word != "tree") throw std::runtime_error("value model: expected 'tree'");
    int nodes = 0, leaves = 0;
    in >> nodes >> leaves;
    Tree tree;
    tree.nodes.resize(nodes);
    tree.leaves.resize(leaves);
    for (Node& n : tree.nodes) in >> n.feature >> n.threshold >> n.left >> n.right;
    for (double& v : tree.leaves) in >> v;
    if (!in || leaves < 1) throw std::runtime_error("value model: truncated tree");
    for (const Node& n : tree.nodes)
      if (n.feature < 0 || n.feature >= kValueFeatures || n.left >= nodes || n.right >= nodes ||
          ~n.left >= leaves || ~n.right >= leaves)
        throw std::runtime_error("value model: bad node");
    model.trees_.push_back(std::move(tree));
  }
  return model;
}

double ValueModel::Predict(const std::array<double, kValueFeatures>& features) const {
  double raw = 0;
  for (const Tree& tree : trees_) {
    if (tree.nodes.empty()) {
      raw += tree.leaves[0];
      continue;
    }
    int i = 0;
    for (;;) {
      const Node& n = tree.nodes[i];
      const int next = features[n.feature] <= n.threshold ? n.left : n.right;
      if (next < 0) {
        raw += tree.leaves[~next];
        break;
      }
      i = next;
    }
  }
  return 1 / (1 + std::exp(-raw));
}

}  // namespace scrabble
