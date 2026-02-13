#pragma once

#include <string>

namespace logiq::utils {

enum class LogLevel { Debug = 0, Info, Warn, Error };

class Logger {
public:
  // Initialize logger with minimum log level
  static void init(LogLevel level);

  static void debug(const std::string &message);
  static void info(const std::string &message);
  static void warn(const std::string &message);
  static void error(const std::string &message);

private:
  static void log(LogLevel level, const std::string &message);
};

} // namespace logiq::utils
