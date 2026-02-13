#pragma once

#include <string>

namespace logiq::config {

struct LoggingConfig {
  std::string level{"info"};
};

struct Config {
  LoggingConfig logging;

  std::string input_path{"logs.log"};
  std::string checkpoint_path{"checkpoint.json"};
};

} // namespace logiq::config
