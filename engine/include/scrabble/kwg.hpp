#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "scrabble/gaddag.hpp"

namespace scrabble {

// Lexicon in the KWG format of wolges and Macondo: a flat array of
// little-endian 32-bit arcs. Each arc holds a tile (bits 24-31, 0 is the GADDAG
// separator, 1..26 are letters), an accepts flag (bit 23: the path ending here
// is a word), an end-of-siblings flag (bit 22) and the index of its children's
// sibling list (bits 0-21, 0 = no children). Arc 0 points to the DAWG root list
// and arc 1 to the GADDAG root list.
class Kwg {
 public:
  using Arc = std::uint32_t;
  static constexpr Arc kNoChildren = 0;

  static Kwg FromBytes(std::string_view bytes);
  static Kwg FromFile(const std::string& path);
  // Builds a minimal KWG (DAWG and GADDAG) for these words; invalid entries are skipped.
  static Kwg FromWords(const std::vector<std::string>& words);

  std::string ToBytes() const;
  void WriteFile(const std::string& path) const;

  static std::uint8_t Tile(Arc a) { return static_cast<std::uint8_t>(a >> 24); }
  static bool Accepts(Arc a) { return (a >> 23) & 1; }
  static bool IsEnd(Arc a) { return (a >> 22) & 1; }
  static std::uint32_t Children(Arc a) { return a & 0x3FFFFF; }

  std::uint32_t DawgRoot() const { return Children(arcs_[0]); }
  std::uint32_t GaddagRoot() const { return Children(arcs_[1]); }
  Arc At(std::uint32_t index) const { return arcs_[index]; }
  std::size_t NumArcs() const { return arcs_.size(); }

  // Index of the arc for `tile` in the sibling list starting at `list`, or
  // kNoChildren if absent.
  std::uint32_t Find(std::uint32_t list, std::uint8_t tile) const;

  // Every word of the DAWG, in alphabetical order.
  std::vector<std::string> Words() const;
  bool Contains(std::string_view word) const;
  // True if the GADDAG accepts this path (letters 'A'-'Z', '@' for the separator).
  bool GaddagAccepts(std::string_view path) const;

  // Our move generator's GADDAG, built from the DAWG's words.
  Gaddag ToGaddag() const { return Gaddag::FromWords(Words()); }

 private:
  std::vector<Arc> arcs_;
};

}  // namespace scrabble
