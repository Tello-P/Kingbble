#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "scrabble/gaddag.hpp"

namespace scrabble {

// The words of a lexicon by their letters, for move generation by anagram (the
// idea of Magpie's word map): a multiset of letters finds every word made of
// exactly those letters. Built from a GADDAG, whose separator-free paths are
// the words reversed.
class AnagramIndex {
 public:
  // A multiset of letters: four bits per letter (A in the lowest), so up to 15
  // of each; the word length is the sum of the counts.
  struct Key {
    std::uint64_t lo = 0, hi = 0;  // letters 1..16, 17..26
    void Add(std::uint8_t letter) {
      if (letter <= 16)
        lo += 1ULL << (4 * (letter - 1));
      else
        hi += 1ULL << (4 * (letter - 17));
    }
    void Remove(std::uint8_t letter) {
      if (letter <= 16)
        lo -= 1ULL << (4 * (letter - 1));
      else
        hi -= 1ULL << (4 * (letter - 17));
    }
    bool operator==(const Key&) const = default;
  };

  static AnagramIndex FromGaddag(const Gaddag& gaddag, int max_length = 15);

  // The words with exactly these letters, each `length` letters (1..26) long,
  // one after another; nullptr and 0 if there are none.
  const std::uint8_t* Find(const Key& key, int length, int& count) const;

  std::size_t NumWords() const { return num_words_; }

 private:
  struct KeyHash {
    std::size_t operator()(const Key& k) const {
      return static_cast<std::size_t>((k.lo * 0x9E3779B97F4A7C15ULL) ^
                                      (k.hi * 0xC2B2AE3D27D4EB4FULL));
    }
  };
  struct Range {
    std::uint32_t first = 0, count = 0;  // offset in letters_, number of words
  };
  std::unordered_map<Key, Range, KeyHash> map_;
  std::vector<std::uint8_t> letters_;
  std::size_t num_words_ = 0;
};

}  // namespace scrabble
