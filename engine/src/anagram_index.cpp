#include "scrabble/anagram_index.hpp"

#include <algorithm>
#include <bit>
#include <string>

namespace scrabble {

namespace {

void Collect(const Gaddag& g, Gaddag::Node node, std::string& reversed, int max_length,
             std::vector<std::string>& out) {
  if (!reversed.empty() && g.IsTerminal(node)) out.emplace_back(reversed.rbegin(), reversed.rend());
  if (static_cast<int>(reversed.size()) == max_length) return;
  for (std::uint32_t bits = g.LetterMask(node) & 0x7FFFFFEu; bits; bits &= bits - 1) {
    const auto L = static_cast<std::uint8_t>(std::countr_zero(bits));
    reversed.push_back(static_cast<char>(L));
    Collect(g, g.Next(node, L), reversed, max_length, out);
    reversed.pop_back();
  }
}

}  // namespace

AnagramIndex AnagramIndex::FromGaddag(const Gaddag& gaddag, int max_length) {
  std::vector<std::string> words;
  std::string reversed;
  Collect(gaddag, gaddag.Root(), reversed, max_length, words);
  // Grouped by letters, so each group's words lie together.
  auto key_of = [](const std::string& w) {
    Key k;
    for (char c : w) k.Add(static_cast<std::uint8_t>(c));
    return k;
  };
  std::vector<std::pair<Key, const std::string*>> keyed;
  keyed.reserve(words.size());
  for (const std::string& w : words) keyed.emplace_back(key_of(w), &w);
  std::sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) {
    if (a.first.hi != b.first.hi) return a.first.hi < b.first.hi;
    if (a.first.lo != b.first.lo) return a.first.lo < b.first.lo;
    return *a.second < *b.second;
  });
  AnagramIndex index;
  std::size_t groups = 0;
  for (std::size_t i = 0; i < keyed.size(); ++i)
    groups += i == 0 || !(keyed[i].first == keyed[i - 1].first);
  std::size_t size = 1024;
  while (size < 2 * groups) size *= 2;
  index.slots_.assign(size, Slot());
  index.slot_mask_ = size - 1;
  for (std::size_t i = 0; i < keyed.size();) {
    Slot slot;
    slot.key = keyed[i].first;
    slot.first = static_cast<std::uint32_t>(index.letters_.size());
    std::size_t j = i;
    for (; j < keyed.size() && keyed[j].first == keyed[i].first; ++j) {
      index.letters_.insert(index.letters_.end(), keyed[j].second->begin(), keyed[j].second->end());
      ++slot.count;
    }
    std::uint64_t s = Hash(slot.key) & index.slot_mask_;
    while (index.slots_[s].count != 0) s = (s + 1) & index.slot_mask_;
    index.slots_[s] = slot;
    i = j;
  }
  index.num_words_ = words.size();
  // The blank table: for each group's letters, each distinct letter left out.
  // Sized for every (group, letter) pair, at most half full.
  auto has = [](const Key& k, std::uint8_t x) {
    return x <= 16 ? (k.lo >> (4 * (x - 1)) & 15) != 0 : (k.hi >> (4 * (x - 17)) & 15) != 0;
  };
  std::size_t pairs = 0;
  for (const Slot& slot : index.slots_)
    if (slot.count)
      for (std::uint8_t x = 1; x <= 26; ++x) pairs += has(slot.key, x);
  std::size_t bsize = 1024;
  while (bsize < 2 * pairs) bsize *= 2;
  index.blank_slots_.assign(bsize, BlankSlot());
  index.blank_mask_ = bsize - 1;
  for (const Slot& slot : index.slots_) {
    if (!slot.count) continue;
    for (std::uint8_t x = 1; x <= 26; ++x) {
      if (!has(slot.key, x)) continue;
      Key less = slot.key;
      less.Remove(x);
      std::uint64_t b = Hash(less) & index.blank_mask_;
      while (index.blank_slots_[b].letters != 0 && !(index.blank_slots_[b].key == less))
        b = (b + 1) & index.blank_mask_;
      if (index.blank_slots_[b].letters == 0) index.blank_slots_[b].key = less;
      index.blank_slots_[b].letters |= 1u << x;
    }
  }
  return index;
}

std::uint32_t AnagramIndex::BlankLetters(const Key& key) const {
  for (std::uint64_t b = Hash(key) & blank_mask_;; b = (b + 1) & blank_mask_) {
    const BlankSlot& slot = blank_slots_[b];
    if (slot.letters == 0) return 0;
    if (slot.key == key) return slot.letters;
  }
}

const std::uint8_t* AnagramIndex::Find(const Key& key, [[maybe_unused]] int length,
                                       int& count) const {
  for (std::uint64_t s = Hash(key) & slot_mask_;; s = (s + 1) & slot_mask_) {
    const Slot& slot = slots_[s];
    if (slot.count == 0) break;
    if (slot.key == key) {
      count = static_cast<int>(slot.count);
      return letters_.data() + slot.first;
    }
  }
  count = 0;
  return nullptr;
}

}  // namespace scrabble
