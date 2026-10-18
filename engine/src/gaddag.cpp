#include "scrabble/gaddag.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace scrabble {

namespace {

bool IsWord(const std::string& w) {
  if (w.size() < 2) return false;
  return std::all_of(w.begin(), w.end(), [](char c) { return c >= 'A' && c <= 'Z'; });
}

}  // namespace

// Incremental construction of a minimal acyclic automaton from sorted input.
class GaddagBuilder {
 public:
  explicit GaddagBuilder(Gaddag& g) : g_(g) { open_.push_back({}); }

  void Add(const std::string& s) {
    std::size_t prefix = 0;
    while (prefix < s.size() && prefix < last_.size() && s[prefix] == last_[prefix]) ++prefix;
    Minimize(prefix);
    for (std::size_t i = prefix; i < s.size(); ++i) {
      open_[i].edges.push_back({static_cast<std::uint8_t>(s[i]), Gaddag::kNone});
      open_.resize(i + 2);
      open_[i + 1] = {};
    }
    open_[s.size()].terminal = true;
    last_ = s;
  }

  void Finish() {
    Minimize(0);
    g_.root_ = Freeze(open_[0]);
  }

 private:
  struct OpenNode {
    bool terminal = false;
    std::vector<Gaddag::Edge> edges;  // target of the last edge is the next open node
  };

  // Freezes open nodes deeper than `depth`, merging them with equivalent ones.
  void Minimize(std::size_t depth) {
    for (std::size_t d = last_.size(); d > depth; --d) {
      open_[d - 1].edges.back().target = Freeze(open_[d]);
    }
    open_.resize(depth + 1);
  }

  Gaddag::Node Freeze(const OpenNode& n) {
    std::string key;
    key.reserve(1 + n.edges.size() * 5);
    key.push_back(n.terminal ? 1 : 0);
    for (const auto& e : n.edges) {
      key.push_back(static_cast<char>(e.letter));
      key.append(reinterpret_cast<const char*>(&e.target), sizeof(e.target));
    }
    auto it = register_.find(key);
    if (it != register_.end()) return it->second;
    Gaddag::Node id = static_cast<Gaddag::Node>(g_.nodes_.size());
    std::uint32_t letters = 0;
    for (const auto& e : n.edges) {
      if (letters >> e.letter) throw std::logic_error("gaddag edges out of order");
      letters |= 1u << e.letter;
    }
    g_.nodes_.push_back({static_cast<std::uint32_t>(g_.edges_.size()), letters,
                         static_cast<std::uint8_t>(n.edges.size()), n.terminal});
    g_.edges_.insert(g_.edges_.end(), n.edges.begin(), n.edges.end());
    register_.emplace(std::move(key), id);
    return id;
  }

  Gaddag& g_;
  std::vector<OpenNode> open_;
  std::string last_;
  std::unordered_map<std::string, Gaddag::Node> register_;
};

Gaddag Gaddag::FromWords(const std::vector<std::string>& words) {
  std::vector<std::string> paths;
  std::size_t n_words = 0;
  for (const auto& w : words) {
    if (!IsWord(w)) continue;
    ++n_words;
    std::string enc(w.size(), 0);
    for (std::size_t i = 0; i < w.size(); ++i) enc[i] = static_cast<char>(w[i] - 'A' + 1);
    for (std::size_t i = 1; i <= enc.size(); ++i) {
      std::string p(enc.rend() - static_cast<std::ptrdiff_t>(i), enc.rend());
      if (i < enc.size()) {
        p.push_back(static_cast<char>(kSeparator));
        p.append(enc.begin() + static_cast<std::ptrdiff_t>(i), enc.end());
      }
      paths.push_back(std::move(p));
    }
  }
  Gaddag g = FromPaths(std::move(paths));
  g.num_words_ = n_words;
  return g;
}

Gaddag Gaddag::FromPaths(std::vector<std::string> paths) {
  std::sort(paths.begin(), paths.end());
  paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
  Gaddag g;
  GaddagBuilder b(g);
  for (const auto& p : paths) b.Add(p);
  b.Finish();
  return g;
}

Gaddag Gaddag::FromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open lexicon: " + path);
  std::vector<std::string> words;
  std::string line;
  while (std::getline(in, line)) {
    auto end = line.find_first_of(" \t\r");
    if (end != std::string::npos) line.resize(end);
    for (auto& c : line)
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    words.push_back(line);
  }
  std::sort(words.begin(), words.end());
  words.erase(std::unique(words.begin(), words.end()), words.end());
  return FromWords(words);
}

Gaddag::Node Gaddag::Next(Node node, std::uint8_t letter) const {
  const FrozenNode& n = nodes_[node];
  const std::uint32_t bit = 1u << letter;
  if (!(n.letters & bit)) return kNone;
  return edges_[n.first_edge + std::popcount(n.letters & (bit - 1))].target;
}

std::vector<std::uint8_t> Gaddag::Letters(Node node) const {
  const FrozenNode& n = nodes_[node];
  std::vector<std::uint8_t> out;
  for (std::uint32_t i = n.first_edge; i < n.first_edge + n.num_edges; ++i)
    out.push_back(edges_[i].letter);
  return out;
}

bool Gaddag::Contains(std::string_view word) const {
  if (word.size() < 2) return false;
  Node n = root_;
  for (auto it = word.rbegin(); it != word.rend() && n != kNone; ++it) {
    auto t = TileFromChar(*it);
    if (!t || *t == kBlank) return false;
    n = Next(n, *t);
  }
  return n != kNone && IsTerminal(n);
}

}  // namespace scrabble
