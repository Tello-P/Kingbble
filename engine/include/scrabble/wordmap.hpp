#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace scrabble {

// Anagram index: words grouped by their alphabetized letters ("AEINRST" ->
// RETAINS, NASTIER, ...). Queries accept '?' for blanks.
class WordMap {
 public:
  // Words must be uppercase A-Z; anything else is skipped.
  static WordMap FromWords(const std::vector<std::string>& words);
  // Reads one word per line (first token), as written by scripts/fetch_lexicon.py.
  static WordMap FromFile(const std::string& path);

  // Words using exactly these tiles, sorted and without duplicates.
  std::vector<std::string> Anagrams(std::string_view tiles) const;
  // Words using any two or more of these tiles, sorted and without duplicates.
  std::vector<std::string> Subanagrams(std::string_view tiles) const;

  bool Contains(std::string_view word) const;
  std::size_t NumWords() const { return num_words_; }
  std::size_t NumKeys() const { return map_.size(); }

 private:
  void Collect(std::string& letters, int blanks, char from, std::vector<std::string>& out) const;

  std::unordered_map<std::string, std::vector<std::string>> map_;
  std::size_t num_words_ = 0;
};

}  // namespace scrabble
