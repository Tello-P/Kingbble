#pragma once

// Resumable runs: one line per finished game, appended and synced to disk as
// soon as the game ends, so a run stopped at any moment loses at most the game
// in progress. Lines start with "game N:" (N from 1); a rerun skips those games.

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

class ResultsFile {
 public:
  ResultsFile() = default;  // no file: nothing is kept
  // read_only: only reads, e.g. to report on a run still going.
  explicit ResultsFile(const std::string& path, bool read_only = false) : path_(path) {
    if (path_.empty()) return;
    std::ifstream in(path_, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // A line cut short by a stop has no newline: drop it.
    const std::size_t end = text.rfind('\n');
    const std::size_t keep = end == std::string::npos ? 0 : end + 1;
    if (keep < text.size()) {
      if (!read_only) std::filesystem::resize_file(path_, keep);
      text.resize(keep);
    }
    std::size_t start = 0;
    while (start < text.size()) {
      const std::size_t nl = text.find('\n', start);
      std::string line = text.substr(start, nl - start);
      start = nl + 1;
      int game = 0;
      if (std::sscanf(line.c_str(), "game %d:", &game) == 1 && done_.insert(game).second)
        lines_.push_back(line);
    }
  }

  bool Done(int game) const { return done_.count(game) > 0; }
  const std::vector<std::string>& lines() const { return lines_; }

  // Appends "game N: ..." and waits until it is on disk.
  void Append(int game, const std::string& line) {
    done_.insert(game);
    lines_.push_back(line);
    if (path_.empty()) return;
    const int fd = open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) throw std::runtime_error("cannot open " + path_);
    const std::string text = line + "\n";
    const bool ok = write(fd, text.data(), text.size()) == static_cast<ssize_t>(text.size());
    fsync(fd);
    close(fd);
    if (!ok) throw std::runtime_error("cannot write " + path_);
  }

 private:
  std::string path_;
  std::set<int> done_;
  std::vector<std::string> lines_;
};
