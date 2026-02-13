#include "ConfigLoader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace logiq::config {

namespace {

// Trim helpers
inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
}

inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

inline std::string trim(std::string s) {
  ltrim(s);
  rtrim(s);
  return s;
}

inline bool starts_with(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Remove quotes if the value is "..." or '...'
inline std::string strip_quotes(std::string v) {
  v = trim(v);
  if (v.size() >= 2) {
    if ((v.front() == '"' && v.back() == '"') ||
        (v.front() == '\'' && v.back() == '\'')) {
      return v.substr(1, v.size() - 2);
    }
  }
  return v;
}

} // namespace

Config ConfigLoader::load(const std::string &path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("ConfigLoader: failed to open config file: " +
                             path);
  }

  Config cfg;
  std::string line;
  std::size_t line_no = 0;

  while (std::getline(in, line)) {
    line_no++;

    // Remove comments (# ...)
    auto hash_pos = line.find('#');
    if (hash_pos != std::string::npos) {
      line = line.substr(0, hash_pos);
    }

    line = trim(line);
    if (line.empty())
      continue;

    // Expect "key: value"
    auto colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
      throw std::runtime_error("ConfigLoader: invalid line (missing ':') at " +
                               std::to_string(line_no));
    }

    std::string key = trim(line.substr(0, colon_pos));
    std::string value = trim(line.substr(colon_pos + 1));
    value = strip_quotes(value);

    if (key.empty()) {
      throw std::runtime_error("ConfigLoader: empty key at line " +
                               std::to_string(line_no));
    }

    apply_kv(cfg, key, value);
  }

  return cfg;
}

void ConfigLoader::apply_kv(Config &cfg, const std::string &key,
                            const std::string &value) {
  // Logging
  if (key == "logging.level" || key == "logging.levels" || key == "log.level") {
    cfg.logging.level = value;
    return;
  }

  // Input
  if (key == "input.path" || key == "input.file" || key == "input") {
    cfg.input_path = value;
    return;
  }

  // Checkpoint
  if (key == "checkpoint.path" || key == "state.checkpoint" ||
      key == "checkpoint") {
    cfg.checkpoint_path = value;
    return;
  }

  // Unknown keys are ignored for forward compatibility.
  // You can switch this to "throw" if you prefer strict configs.
}

} // namespace logiq::config
