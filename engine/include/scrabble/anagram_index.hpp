#pragma once

#include <cstdint>
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
  // Open addressing over a power-of-two table (a slot with count 0 is free),
  // so a lookup is a hash and usually one cache line.
  struct Slot {
    Key key;
    std::uint32_t first = 0, count = 0;  // offset in letters_, number of words
  };
  static std::uint64_t Hash(const Key& k) {
    std::uint64_t h =
        k.lo * 0x9E3779B97F4A7C15ULL ^ (k.hi + 0x632BE59BD9B4E019ULL) * 0xC2B2AE3D27D4EB4FULL;
    return h ^ (h >> 31);
  }
  std::vector<Slot> slots_;
  std::uint64_t slot_mask_ = 0;
  std::vector<std::uint8_t> letters_;
  std::size_t num_words_ = 0;
};

}  // namespace scrabble
