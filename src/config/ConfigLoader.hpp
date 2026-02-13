#pragma once

#include "Config.hpp"
#include <string>

namespace logiq::config {

// Minimal, dependency-free config loader.
// Supports a simple YAML-like format with "key: value" pairs, for example:
//
// logging.level: debug
// input.path: logs.log
// checkpoint.path: checkpoint.json
//
class ConfigLoader {
public:
  // Loads config from a file path.
  // Throws std::runtime_error on failure.
  static Config load(const std::string &path);

private:
  static void apply_kv(Config &cfg, const std::string &key,
                       const std::string &value);
};

} // namespace logiq::config