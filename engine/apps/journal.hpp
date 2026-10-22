#pragma once

// Resumable output for long runs. Work is done in units (a game file, a played
// game): each unit's records are appended to the records file as a whole, then
// a line goes to <records>.done with the unit's key, the records size after it
// and its results. A rerun truncates the records to the last finished unit and
// skips the units already done.

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace scrabble {

class Journal {
 public:
  explicit Journal(const std::string& records_path)
      : records_path_(records_path), done_path_(records_path + ".done") {
    namespace fs = std::filesystem;
    std::uintmax_t size = 0;
    std::ifstream in(done_path_);
    for (std::string line; std::getline(in, line);) {
      std::istringstream ss(line);
      std::string key;
      std::uintmax_t offset = 0;
      if (!(ss >> key >> offset)) break;  // a torn last line
      std::vector<double> values;
      for (double x; ss >> x;) values.push_back(x);
      done_[key] = std::move(values);
      size = offset;
    }
    std::error_code ec;
    if (fs::exists(records_path_, ec) && fs::file_size(records_path_, ec) > size)
      fs::resize_file(records_path_, size, ec);
    if (done_.empty()) std::ofstream(done_path_, std::ios::trunc);
    records_.open(records_path_, std::ios::app);
    done_out_.open(done_path_, std::ios::app);
  }

  bool ok() const { return records_.good() && done_out_.good(); }
  // Results of the finished units, by key.
  const std::map<std::string, std::vector<double>>& done() const { return done_; }
  bool IsDone(const std::string& key) const { return done_.count(key) > 0; }
  // Records of the unit in progress.
  std::ostream& unit() { return buffer_; }

  void Finish(const std::string& key, const std::vector<double>& values) {
    records_ << buffer_.str();
    records_.flush();
    buffer_.str("");
    done_out_ << key << '\t' << std::filesystem::file_size(records_path_);
    for (double x : values) done_out_ << '\t' << x;
    done_out_ << '\n';
    done_out_.flush();
    done_[key] = values;
  }

 private:
  std::string records_path_, done_path_;
  std::map<std::string, std::vector<double>> done_;
  std::ofstream records_, done_out_;
  std::ostringstream buffer_;
};

}  // namespace scrabble
