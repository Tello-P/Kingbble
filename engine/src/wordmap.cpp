#include "scrabble/wordmap.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace scrabble {

namespace {

bool ValidWord(const std::string& w) {
  return w.size() >= 2 &&
         std::all_of(w.begin(), w.end(), [](char c) { return c >= 'A' && c <= 'Z'; });
}

// Splits a query into its sorted letters and its number of blanks.
bool ParseTiles(std::string_view tiles, std::string& letters, int& blanks) {
  letters.clear();
  blanks = 0;
  for (char c : tiles) {
    if (c == '?') {
      ++blanks;
    } else if (c >= 'A' && c <= 'Z') {
      letters.push_back(c);
    } else if (c >= 'a' && c <= 'z') {
      letters.push_back(static_cast<char>(c - 'a' + 'A'));
    } else {
      return false;
    }
  }
  std::sort(letters.begin(), letters.end());
  return true;
}

void SortUnique(std::vector<std::string>& v) {
  std::sort(v.begin(), v.end());
  v.erase(std::unique(v.begin(), v.end()), v.end());
}

}  // namespace

WordMap WordMap::FromWords(const std::vector<std::string>& words) {
  WordMap wm;
  for (const std::string& w : words) {
    if (!ValidWord(w)) continue;
    std::string key = w;
    std::sort(key.begin(), key.end());
    auto& bucket = wm.map_[key];
    if (std::find(bucket.begin(), bucket.end(), w) != bucket.end()) continue;
    bucket.push_back(w);
    ++wm.num_words_;
  }
  for (auto& [key, bucket] : wm.map_) std::sort(bucket.begin(), bucket.end());
  return wm;
}

WordMap WordMap::FromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open lexicon: " + path);
  std::vector<std::string> words;
  std::string line;
  while (std::getline(in, line)) {
    std::size_t end = line.find_first_of(" \t\r");
    words.push_back(line.substr(0, end));
  }
  return FromWords(words);
}

// Fills the blanks with letters >= `from` (non-decreasing, so each multiset once).
void WordMap::Collect(std::string& letters, int blanks, char from,
                      std::vector<std::string>& out) const {
  if (blanks == 0) {
    std::string key = letters;
    std::sort(key.begin(), key.end());
    auto it = map_.find(key);
    if (it != map_.end()) out.insert(out.end(), it->second.begin(), it->second.end());
    return;
  }
  for (char c = from; c <= 'Z'; ++c) {
    letters.push_back(c);
    Collect(letters, blanks - 1, c, out);
    letters.pop_back();
  }
}

std::vector<std::string> WordMap::Anagrams(std::string_view tiles) const {
  std::string letters;
  int blanks;
  std::vector<std::string> out;
  if (!ParseTiles(tiles, letters, blanks)) return out;
  Collect(letters, blanks, 'A', out);
  SortUnique(out);
  return out;
}

std::vector<std::string> WordMap::Subanagrams(std::string_view tiles) const {
  std::string letters;
  int blanks;
  std::vector<std::string> out;
  if (!ParseTiles(tiles, letters, blanks)) return out;
  // Distinct letters with multiplicities; enumerate every sub-multiset.
  std::vector<std::pair<char, int>> groups;
  for (char c : letters) {
    if (groups.empty() || groups.back().first != c) groups.emplace_back(c, 0);
    ++groups.back().second;
  }
  std::vector<int> take(groups.size(), 0);
  std::string sub;
  while (true) {
    sub.clear();
    for (std::size_t g = 0; g < groups.size(); ++g) sub.append(take[g], groups[g].first);
    for (int b = 0; b <= blanks; ++b) {
      if (sub.size() + b >= 2) Collect(sub, b, 'A', out);
    }
    std::size_t g = 0;
    while (g < groups.size() && take[g] == groups[g].second) take[g++] = 0;
    if (g == groups.size()) break;
    ++take[g];
  }
  SortUnique(out);
  return out;
}

bool WordMap::Contains(std::string_view word) const {
  std::string key(word);
  std::sort(key.begin(), key.end());
  auto it = map_.find(key);
  return it != map_.end() &&
         std::binary_search(it->second.begin(), it->second.end(), std::string(word));
}

}  // namespace scrabble
