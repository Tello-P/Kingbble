// Writes a KWG lexicon (wolges / Macondo format) from a word list.
//
//   kingbble_buildkwg lexica/ENABLE.txt lexica/ENABLE.kwg

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "scrabble/kwg.hpp"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " WORDS.txt OUT.kwg\n";
    return 2;
  }
  std::ifstream in(argv[1]);
  if (!in) {
    std::cerr << "cannot open " << argv[1] << "\n";
    return 1;
  }
  std::vector<std::string> words;
  for (std::string line; std::getline(in, line);) {
    line = line.substr(0, line.find_first_of(" \t\r"));
    for (auto& c : line)
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    words.push_back(line);
  }
  std::sort(words.begin(), words.end());
  words.erase(std::unique(words.begin(), words.end()), words.end());
  scrabble::Kwg kwg = scrabble::Kwg::FromWords(words);
  kwg.WriteFile(argv[2]);
  std::cout << kwg.Words().size() << " words, " << kwg.NumArcs() << " arcs\n";
  return 0;
}
