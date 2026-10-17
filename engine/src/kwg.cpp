#include "scrabble/kwg.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace scrabble {

namespace {

constexpr std::uint32_t kMaxIndex = 0x3FFFFF;
constexpr std::uint32_t kAcceptsBit = 1u << 23;
constexpr std::uint32_t kEndBit = 1u << 22;

std::uint8_t KwgTile(std::uint8_t gaddag_letter) {
  return gaddag_letter == Gaddag::kSeparator ? 0 : gaddag_letter;
}

// Lays out the sibling lists of one or more automata, sharing identical lists.
class KwgWriter {
 public:
  explicit KwgWriter(std::vector<Kwg::Arc>& arcs) : arcs_(arcs) {}

  // Returns the index of the list of `node`'s outgoing arcs (kNoChildren if none).
  std::uint32_t Emit(const Gaddag& g, Gaddag::Node node) {
    auto key = std::make_pair(&g, node);
    if (auto it = done_.find(key); it != done_.end()) return it->second;
    std::vector<std::uint8_t> letters = g.Letters(node);
    std::vector<std::pair<std::uint8_t, Kwg::Arc>> list;  // (kwg tile, arc without end bit)
    for (std::uint8_t l : letters) {
      Gaddag::Node next = g.Next(node, l);
      std::uint32_t children = Emit(g, next);
      Kwg::Arc arc = (static_cast<Kwg::Arc>(KwgTile(l)) << 24) |
                     (g.IsTerminal(next) ? kAcceptsBit : 0) | children;
      list.emplace_back(KwgTile(l), arc);
    }
    std::uint32_t index = Kwg::kNoChildren;
    if (!list.empty()) {
      std::sort(list.begin(), list.end());
      std::string sig;
      for (auto& [tile, arc] : list) sig.append(reinterpret_cast<const char*>(&arc), sizeof(arc));
      if (auto it = lists_.find(sig); it != lists_.end()) {
        index = it->second;
      } else {
        index = static_cast<std::uint32_t>(arcs_.size());
        if (index + list.size() > kMaxIndex) throw std::length_error("lexicon too large for KWG");
        for (auto& [tile, arc] : list) arcs_.push_back(arc);
        arcs_.back() |= kEndBit;
        lists_.emplace(std::move(sig), index);
      }
    }
    done_.emplace(key, index);
    return index;
  }

 private:
  struct KeyHash {
    std::size_t operator()(const std::pair<const Gaddag*, Gaddag::Node>& k) const {
      return std::hash<const void*>()(k.first) ^ (std::hash<Gaddag::Node>()(k.second) * 31);
    }
  };

  std::vector<Kwg::Arc>& arcs_;
  std::unordered_map<std::pair<const Gaddag*, Gaddag::Node>, std::uint32_t, KeyHash> done_;
  std::unordered_map<std::string, std::uint32_t> lists_;
};

void CollectWords(const Kwg& kwg, std::uint32_t list, std::string& prefix,
                  std::vector<std::string>& out) {
  for (std::uint32_t i = list;; ++i) {
    Kwg::Arc a = kwg.At(i);
    prefix.push_back(static_cast<char>('A' + Kwg::Tile(a) - 1));
    if (Kwg::Accepts(a)) out.push_back(prefix);
    if (Kwg::Children(a) != Kwg::kNoChildren) CollectWords(kwg, Kwg::Children(a), prefix, out);
    prefix.pop_back();
    if (Kwg::IsEnd(a)) break;
  }
}

}  // namespace

Kwg Kwg::FromBytes(std::string_view bytes) {
  if (bytes.size() % 4 != 0 || bytes.size() < 8) throw std::runtime_error("malformed KWG");
  Kwg k;
  k.arcs_.resize(bytes.size() / 4);
  for (std::size_t i = 0; i < k.arcs_.size(); ++i) {
    const auto* b = reinterpret_cast<const unsigned char*>(bytes.data() + 4 * i);
    k.arcs_[i] = static_cast<Arc>(b[0]) | static_cast<Arc>(b[1]) << 8 |
                 static_cast<Arc>(b[2]) << 16 | static_cast<Arc>(b[3]) << 24;
  }
  for (std::size_t i = 0; i < k.arcs_.size(); ++i) {
    if (Children(k.arcs_[i]) >= k.arcs_.size()) throw std::runtime_error("malformed KWG");
  }
  return k;
}

Kwg Kwg::FromFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open KWG: " + path);
  std::stringstream ss;
  ss << in.rdbuf();
  return FromBytes(ss.str());
}

Kwg Kwg::FromWords(const std::vector<std::string>& words) {
  std::vector<std::string> dawg_paths;
  for (const auto& w : words) {
    if (w.size() < 2 ||
        !std::all_of(w.begin(), w.end(), [](char c) { return c >= 'A' && c <= 'Z'; }))
      continue;
    std::string p(w.size(), 0);
    for (std::size_t i = 0; i < w.size(); ++i) p[i] = static_cast<char>(w[i] - 'A' + 1);
    dawg_paths.push_back(std::move(p));
  }
  Gaddag dawg = Gaddag::FromPaths(std::move(dawg_paths));
  Gaddag gaddag = Gaddag::FromWords(words);

  Kwg k;
  k.arcs_.assign(2, kEndBit);
  KwgWriter writer(k.arcs_);
  std::uint32_t dawg_root = writer.Emit(dawg, dawg.Root());
  std::uint32_t gaddag_root = writer.Emit(gaddag, gaddag.Root());
  k.arcs_[0] = kEndBit | dawg_root;
  k.arcs_[1] = kEndBit | gaddag_root;
  return k;
}

std::string Kwg::ToBytes() const {
  std::string out(arcs_.size() * 4, '\0');
  for (std::size_t i = 0; i < arcs_.size(); ++i) {
    for (int b = 0; b < 4; ++b) out[4 * i + b] = static_cast<char>((arcs_[i] >> (8 * b)) & 0xFF);
  }
  return out;
}

void Kwg::WriteFile(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("cannot write KWG: " + path);
  std::string bytes = ToBytes();
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::uint32_t Kwg::Find(std::uint32_t list, std::uint8_t tile) const {
  if (list == kNoChildren) return kNoChildren;
  for (std::uint32_t i = list;; ++i) {
    if (Tile(arcs_[i]) == tile) return i;
    if (IsEnd(arcs_[i])) return kNoChildren;
  }
}

std::vector<std::string> Kwg::Words() const {
  std::vector<std::string> out;
  std::string prefix;
  if (DawgRoot() != kNoChildren) CollectWords(*this, DawgRoot(), prefix, out);
  return out;
}

bool Kwg::Contains(std::string_view word) const {
  std::uint32_t list = DawgRoot(), arc = kNoChildren;
  for (char c : word) {
    if (c < 'A' || c > 'Z') return false;
    arc = Find(list, static_cast<std::uint8_t>(c - 'A' + 1));
    if (arc == kNoChildren) return false;
    list = Children(arcs_[arc]);
  }
  return arc != kNoChildren && Accepts(arcs_[arc]);
}

bool Kwg::GaddagAccepts(std::string_view path) const {
  std::uint32_t list = GaddagRoot(), arc = kNoChildren;
  for (char c : path) {
    std::uint8_t tile;
    if (c == '@') {
      tile = 0;
    } else if (c >= 'A' && c <= 'Z') {
      tile = static_cast<std::uint8_t>(c - 'A' + 1);
    } else {
      return false;
    }
    arc = Find(list, tile);
    if (arc == kNoChildren) return false;
    list = Children(arcs_[arc]);
  }
  return arc != kNoChildren && Accepts(arcs_[arc]);
}

}  // namespace scrabble
