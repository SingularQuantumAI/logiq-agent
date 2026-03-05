#include "retry/RetryScheduler.hpp"

#include <algorithm>

namespace logiq::retry {

RetryScheduler::RetryScheduler(RetryPolicy policy)
    : policy_(policy), rng_(std::random_device{}()) {}

std::chrono::milliseconds RetryScheduler::jitter() {
  if (policy_.jitter_ms == 0)
    return std::chrono::milliseconds{0};
  std::uniform_int_distribution<std::uint32_t> dist(0, policy_.jitter_ms);
  return std::chrono::milliseconds{dist(rng_)};
}

std::chrono::milliseconds
RetryScheduler::compute_delay(std::uint32_t attempts) {
  // attempts starts at 1 for first retry
  if (attempts == 0)
    attempts = 1;

  std::uint64_t base = policy_.base_delay_ms;
  std::uint64_t maxd = policy_.max_delay_ms;

  // Exponential: base * 2^(attempts-1), capped
  std::uint64_t factor =
      1ULL << std::min<std::uint32_t>(attempts - 1, 20); // cap shift
  std::uint64_t delay = base * factor;
  delay = std::min<std::uint64_t>(delay, maxd);

  return std::chrono::milliseconds{delay} + jitter();
}

bool RetryScheduler::enqueue(logiq::Batch batch, const std::string &error) {
  if (policy_.max_attempts == 0)
    return false; // retry disabled

  if (q_.size() >= policy_.max_queue_batches) {
    // Drop oldest to keep bounded memory (you can change policy later)
    q_.pop_front();
  }

  PendingBatch pb;
  pb.batch = std::move(batch);
  pb.attempts = 1;
  pb.last_error = error;
  pb.next_attempt =
      std::chrono::steady_clock::now() + compute_delay(pb.attempts);

  q_.push_back(std::move(pb));
  return true;
}

std::optional<PendingBatch> RetryScheduler::pop_ready() {
  if (q_.empty())
    return std::nullopt;

  auto now = std::chrono::steady_clock::now();
  if (q_.front().next_attempt > now)
    return std::nullopt;

  PendingBatch pb = std::move(q_.front());
  q_.pop_front();
  return pb;
}

void RetryScheduler::requeue(PendingBatch pb) {
  if (policy_.max_attempts == 0)
    return;

  if (pb.attempts >= policy_.max_attempts) {
    // Give up permanently.
    return;
  }

  pb.attempts++;
  pb.next_attempt =
      std::chrono::steady_clock::now() + compute_delay(pb.attempts);
  q_.push_back(std::move(pb));
}

void RetryScheduler::on_success() {
  // No-op for now (hook for future metrics)
}

} // namespace logiq::retry
