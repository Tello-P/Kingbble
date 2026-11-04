#pragma once

// The manifest of a run, kept next to its results file as <results>.manifest.json:
// what produced the results (tags such as the commit, given by the caller),
// everything the games depend on ("config": seeds, bots, parameters and the
// hashes of the lexicon, leaves and model files) and the command line. A rerun
// that resumes the results file must have the same config, so one file never
// mixes games played under different settings.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace scrabble {

// FNV-1a 64 of a file's bytes as 16 hex digits, "missing" if it cannot be read.
inline std::string FileHash(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return "missing";
  std::uint64_t h = 0xcbf29ce484222325ULL;
  char buf[1 << 16];
  while (in.read(buf, sizeof buf) || in.gcount() > 0) {
    for (std::streamsize i = 0; i < in.gcount(); ++i) {
      h ^= static_cast<unsigned char>(buf[i]);
      h *= 0x100000001b3ULL;
    }
  }
  char out[17];
  std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(h));
  return out;
}

inline std::string JsonString(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char esc[8];
      std::snprintf(esc, sizeof esc, "\\u%04x", c);
      out += esc;
    } else {
      out += c;
    }
  }
  return out + "\"";
}

class Manifest {
 public:
  // A config entry; entries keep their order.
  void Set(const std::string& key, const std::string& value) { config_.emplace_back(key, value); }
  void Set(const std::string& key, double value) {
    std::ostringstream ss;
    ss << value;
    Set(key, ss.str());
  }
  void Tag(const std::string& key, const std::string& value) { tags_[key] = value; }
  void SetArgv(int argc, char** argv) { argv_.assign(argv, argv + argc); }

  // The config as one JSON line, the part a resumed run must match.
  std::string ConfigJson() const {
    std::string out = "{";
    for (std::size_t i = 0; i < config_.size(); ++i)
      out += (i ? "," : "") + JsonString(config_[i].first) + ":" + JsonString(config_[i].second);
    return out + "}";
  }

  std::string Json() const {
    std::string out = "{\n  \"tags\": {";
    bool first = true;
    for (const auto& [k, v] : tags_) {
      out += (first ? "" : ", ") + JsonString(k) + ": " + JsonString(v);
      first = false;
    }
    out += "},\n  \"config\": " + ConfigJson() + ",\n  \"argv\": [";
    for (std::size_t i = 0; i < argv_.size(); ++i) out += (i ? ", " : "") + JsonString(argv_[i]);
    return out + "]\n}\n";
  }

  // The config line of an existing manifest, empty if there is none.
  static std::string ReadConfig(const std::string& path) {
    std::ifstream in(path);
    const std::string prefix = "  \"config\": ";
    for (std::string line; std::getline(in, line);)
      if (line.rfind(prefix, 0) == 0) {
        std::string config = line.substr(prefix.size());
        if (!config.empty() && config.back() == ',') config.pop_back();
        return config;
      }
    return "";
  }

  bool Write(const std::string& path) const {
    std::ofstream out(path);
    out << Json();
    return static_cast<bool>(out);
  }

 private:
  std::vector<std::pair<std::string, std::string>> config_;
  std::map<std::string, std::string> tags_;
  std::vector<std::string> argv_;
};

}  // namespace scrabble
