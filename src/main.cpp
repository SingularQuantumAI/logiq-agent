#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

#include "config/ConfigLoader.hpp"
#include "core/Agent.hpp"
#include "utils/Logger.hpp"

namespace {

std::atomic<bool> g_running{true};

/**
 * Signal handler for graceful shutdown.
 */
void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    g_running.store(false);
  }
}

} // namespace

static logiq::utils::LogLevel parse_level(const std::string &s) {
  std::string v;
  v.reserve(s.size());
  for (unsigned char c : s)
    v.push_back(static_cast<char>(std::tolower(c)));

  if (v == "debug")
    return logiq::utils::LogLevel::Debug;
  if (v == "info")
    return logiq::utils::LogLevel::Info;
  if (v == "warn" || v == "warning")
    return logiq::utils::LogLevel::Warn;
  if (v == "error")
    return logiq::utils::LogLevel::Error;

  return logiq::utils::LogLevel::Info;
}

int main(int argc, char *argv[]) {
  try {
    // ---------------------------------------------------------
    // 1. Install signal handlers for graceful shutdown
    // ---------------------------------------------------------
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ---------------------------------------------------------
    // 2. Load configuration
    // ---------------------------------------------------------
    std::string config_path = "config/example-config.yaml";

    if (argc > 1) {
      config_path = argv[1];
    }

    auto config = logiq::config::ConfigLoader::load(config_path);

    // ---------------------------------------------------------
    // 3. Initialize logging subsystem
    // ---------------------------------------------------------
    logiq::utils::Logger::init(parse_level(config.logging.level));

    logiq::utils::Logger::info("Starting LogIQ Agent...");
    logiq::utils::Logger::info("Using configuration file: " + config_path);

    // ---------------------------------------------------------
    // 4. Create and initialize Agent
    // ---------------------------------------------------------
    auto agent = std::make_unique<logiq::core::Agent>(config);

    if (!agent->initialize()) {
      logiq::utils::Logger::error("Agent initialization failed.");
      return EXIT_FAILURE;
    }

    logiq::utils::Logger::info("Agent initialized successfully.");

    // ---------------------------------------------------------
    // 5. Run main processing loop
    // ---------------------------------------------------------
    while (g_running.load()) {
      agent->run_once();

      // Prevent tight CPU loop
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // ---------------------------------------------------------
    // 6. Shutdown sequence
    // ---------------------------------------------------------
    logiq::utils::Logger::info("Shutting down LogIQ Agent...");

    agent->shutdown();

    logiq::utils::Logger::info("Shutdown complete.");

    return EXIT_SUCCESS;

  } catch (const std::exception &ex) {
    std::cerr << "Fatal error: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Unknown fatal error occurred." << std::endl;
    return EXIT_FAILURE;
  }
}
