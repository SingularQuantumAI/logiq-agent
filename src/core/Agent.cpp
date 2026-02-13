#include "Agent.hpp"

#include "../config/Config.hpp"
// #include "utils/Logger.hpp"
#include "../utils/Logger.hpp"

namespace logiq::core {

Agent::Agent(const logiq::config::Config &config) : config_(config) {}

bool Agent::initialize() {
  // Initialize subsystems here:
  // - FileFollower
  // - Router
  // - Sinks
  // - CheckpointStore
  // For now, this is a stub.

  logiq::utils::Logger::info("Agent initialize() called.");
  return true;
}

void Agent::run_once() {
  // This will later contain the runtime state machine tick:
  // OBSERVE → READ → FRAME → ROUTE → SEND → ACK

  // For now, just log once per tick (temporary stub).
  logiq::utils::Logger::debug("Agent run_once() tick.");
}

void Agent::shutdown() {
  // Graceful shutdown logic:
  // - Flush batches
  // - Persist checkpoint
  // - Close file descriptors

  logiq::utils::Logger::info("Agent shutdown() called.");
}

} // namespace logiq::core
