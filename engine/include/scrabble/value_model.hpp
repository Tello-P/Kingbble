#pragma once

#include <array>
#include <string>
#include <vector>

#include "scrabble/game.hpp"
#include "scrabble/leaves.hpp"

namespace scrabble {

// Features of a position from the side of the player to move, in the order of
// FEATURES in scripts/train_value.py.
inline constexpr int kValueFeatures = 18;
std::array<double, kValueFeatures> ValueFeatures(const Game& game, const Leaves& leaves);

// A gradient-boosted model of the chance that the player to move wins, trained by
// scripts/train_value.py and written in its text format: a header line
// "kingbble-value 1 <features>", then for every tree a line "tree <nodes> <leaves>",
// one line "feature threshold left right" per node (a child below 0 is leaf ~child,
// the left one taken when the feature is at most the threshold) and a line of leaf
// values. The chance is the logistic of the summed leaf values.
class ValueModel {
 public:
  static ValueModel FromFile(const std::string& path);
  static ValueModel FromText(const std::string& text);

  double Predict(const std::array<double, kValueFeatures>& features) const;
  double Win(const Game& game, const Leaves& leaves) const {
    return Predict(ValueFeatures(game, leaves));
  }

 private:
  struct Node {
    int feature;
    double threshold;
    int left, right;
  };
  struct Tree {
    std::vector<Node> nodes;
    std::vector<double> leaves;
  };
  std::vector<Tree> trees_;
};

}  // namespace scrabble
