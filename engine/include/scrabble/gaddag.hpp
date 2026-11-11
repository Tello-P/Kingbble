#pragma once

#include <bit>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "scrabble/tiles.hpp"

namespace scrabble {

// Minimized GADDAG (Gordon, 1994) stored as flat arrays.
//
// Every word w of length n is inserted as n paths: for each split point i in
// [1, n], reverse(w[0, i)) followed by the separator and w[i, n). The path with
// i == n has no separator. Letters are 1..26 and the separator is kSeparator.
// Shared suffixes are merged during construction (Daciuk et al., 2000), so the
// structure is a minimal acyclic automaton over these paths.
class Gaddag {
 public:
  using Node = std::uint32_t;
  static constexpr Node kNone = 0xFFFFFFFF;
  static constexpr std::uint8_t kSeparator = 27;

  // Words must be uppercase A-Z; anything else is skipped.
  static Gaddag FromWords(const std::vector<std::string>& words);
  // Reads one word per line (first token), as written by scripts/fetch_lexicon.py.
  static Gaddag FromFile(const std::string& path);
  // Minimal automaton accepting exactly these paths (bytes are letters 1..26 or
  // kSeparator). NumWords() is left at zero.
  static Gaddag FromPaths(std::vector<std::string> paths);

  Node Root() const { return root_; }
  // Outgoing letters of a node, in increasing order.
  std::vector<std::uint8_t> Letters(Node node) const;
  Node Next(Node node, std::uint8_t letter) const {
    const FrozenNode& n = nodes_[node];
    const std::uint32_t bit = 1u << letter;
    if (!(n.letters & bit)) return kNone;
    return targets_[n.first_edge + std::popcount(n.letters & (bit - 1))];
  }
  // Bit L set for each outgoing letter L.
  std::uint32_t LetterMask(Node node) const { return nodes_[node].letters & ~kTerminalBit; }
  bool IsTerminal(Node node) const { return nodes_[node].letters & kTerminalBit; }

  // Dictionary lookup through the separator-free reversed path.
  bool Contains(std::string_view word) const;

  std::size_t NumNodes() const { return nodes_.size(); }
  std::size_t NumEdges() const { return targets_.size(); }
  std::size_t NumWords() const { return num_words_; }

 private:
  // 8 bytes a node: its first edge, and a bit per outgoing letter (edges sorted
  // by letter, so an edge's place is the count of lower letters) with the
  // terminal flag in the top bit; an edge is just its target.
  static constexpr std::uint32_t kTerminalBit = 1u << 31;
  struct FrozenNode {
    std::uint32_t first_edge;
    std::uint32_t letters;
  };
  struct Edge {
    std::uint8_t letter;
    Node target;
  };
  friend class GaddagBuilder;

  std::vector<FrozenNode> nodes_;
  std::vector<Node> targets_;
  Node root_ = kNone;
  std::size_t num_words_ = 0;
};

}  // namespace scrabble
