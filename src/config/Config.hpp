#pragma once

#include <cstdint>
#include <string>

namespace logiq::config {

struct LoggingConfig {
  std::string level{"info"};
};

struct BatchingConfig {
  std::uint32_t max_records{500};
  std::uint32_t max_bytes{256 * 1024};  // 256KB
  std::uint32_t flush_interval_ms{200}; // 200ms
};

struct RetryConfig {
  // Maximum attempts per batch (e.g., 10). 0 disables retry.
  std::uint32_t max_attempts{10};

  // Exponential backoff base delay.
  std::uint32_t base_delay_ms{200};

  // Maximum backoff delay.
  std::uint32_t max_delay_ms{10'000};

  // Random jitter in milliseconds added to each delay (0 disables jitter).
  std::uint32_t jitter_ms{250};

  // Max queued batches waiting for retry (memory cap).
  std::uint32_t max_queue_batches{256};
};

struct Config {
  LoggingConfig logging;
  BatchingConfig batching;

  std::string input_path{"logs.log"};
  std::string checkpoint_path{"checkpoint.json"};

  RetryConfig retry;
};

} // namespace logiq::config
