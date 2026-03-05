#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <random>
#include <string>

#include "sinks/Sink.hpp"

namespace logiq::retry {

struct RetryPolicy {
  std::uint32_t max_attempts{10};
  std::uint32_t base_delay_ms{200};
  std::uint32_t max_delay_ms{10'000};
  std::uint32_t jitter_ms{250};
  std::uint32_t max_queue_batches{256};
};

struct PendingBatch {
  logiq::Batch batch;
  std::uint32_t attempts{0};
  std::chrono::steady_clock::time_point next_attempt{};
  std::string last_error;
};

class RetryScheduler {
public:
  explicit RetryScheduler(RetryPolicy policy);

  // Enqueue a failed batch for retry. Returns false if dropped (queue full or
  // retry disabled).
  bool enqueue(logiq::Batch batch, const std::string &error);

  // Get the next batch that is ready to be retried (front-only scheduling).
  // Returns nullopt if none are ready.
  std::optional<PendingBatch> pop_ready();

  // Put a batch back (after a failed retry) with updated scheduling.
  void requeue(PendingBatch pb);

  // Mark success: nothing to do (caller commits checkpoint).
  void on_success();

  // Stats
  std::size_t size() const noexcept { return q_.size(); }
  const RetryPolicy &policy() const noexcept { return policy_; }

private:
  RetryPolicy policy_;
  std::deque<PendingBatch> q_;
  std::mt19937 rng_;

private:
  std::chrono::milliseconds compute_delay(std::uint32_t attempts);
  std::chrono::milliseconds jitter();
};

} // namespace logiq::retry
