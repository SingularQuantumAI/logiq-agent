#include "Logger.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace logiq::utils {

namespace {

LogLevel g_level = LogLevel::Info;
std::mutex g_mutex;

// Convert log level to string
std::string level_to_string(LogLevel level) {
  switch (level) {
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warn:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

// Get current timestamp in ISO8601 format
std::string current_timestamp() {
  using namespace std::chrono;

  auto now = system_clock::now();
  auto time = system_clock::to_time_t(now);

  std::tm tm{};
#if defined(__APPLE__)
  localtime_r(&time, &tm);
#else
  localtime_r(&time, &tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

} // namespace

void Logger::init(LogLevel level) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_level = level;
}

void Logger::log(LogLevel level, const std::string &message) {
  if (static_cast<int>(level) < static_cast<int>(g_level)) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_mutex);

  std::cout << "[" << current_timestamp() << "] "
            << "[" << level_to_string(level) << "] " << message << std::endl;
}

void Logger::debug(const std::string &message) {
  log(LogLevel::Debug, message);
}

void Logger::info(const std::string &message) { log(LogLevel::Info, message); }

void Logger::warn(const std::string &message) { log(LogLevel::Warn, message); }

void Logger::error(const std::string &message) {
  log(LogLevel::Error, message);
}

} // namespace logiq::utils
