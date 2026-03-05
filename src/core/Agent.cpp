// File: src/core/Agent.cpp
#include "core/Agent.hpp"

#include "checkpoint/CheckpointStore.hpp"
#include "utils/Logger.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace logiq::core {

namespace {

// Simple monotonic batch id generator (good enough for MVP).
std::string next_batch_id() {
  static std::uint64_t seq = 0;
  return "b-" + std::to_string(++seq);
}

} // namespace

Agent::Agent(const logiq::config::Config &config)
    : config_(config), follower_(config.input_path),
      sink_({
          .name = "primary",
          .url = config.http.url,
          .timeout_ms = static_cast<int>(config.http.timeout_ms),
          .connect_timeout_ms =
              static_cast<int>(config.http.connect_timeout_ms),
          .assume_durable_on_200 = config.http.assume_durable_on_200,
      }),
      checkpoint_store_(config.checkpoint_path),
      retry_(logiq::retry::RetryPolicy{
          .max_attempts = config.retry.max_attempts,
          .base_delay_ms = config.retry.base_delay_ms,
          .max_delay_ms = config.retry.max_delay_ms,
          .jitter_ms = config.retry.jitter_ms,
          .max_queue_batches = config.retry.max_queue_batches,
      }),
      batcher_(logiq::batching::Policy{
          .max_records = config.batching.max_records,
          .max_bytes = config.batching.max_bytes,
          .flush_interval =
              std::chrono::milliseconds(config.batching.flush_interval_ms),
      }) {}

bool Agent::initialize() {
  follower_.open_if_exists();

  // Load checkpoint (if any) and resume only if it matches the current file
  // identity.
  if (follower_.has_fd()) {
    batcher_.reset(follower_.active_id(), follower_.generation());
  }
  try {
    auto cp_opt = checkpoint_store_.load();
    if (cp_opt && follower_.has_fd()) {
      const auto cp = *cp_opt;
      const auto active = follower_.active_id();

      if (active.dev == cp.file_id.dev && active.ino == cp.file_id.ino) {
        follower_.set_position(cp.committed_offset, cp.generation);
        committed_offset_ = cp.committed_offset;
        committed_generation_ = cp.generation;

        logiq::utils::Logger::info(
            "Checkpoint loaded. Resuming at committed_offset=" +
            std::to_string(committed_offset_) +
            " generation=" + std::to_string(committed_generation_));
      } else {
        committed_offset_ = 0;
        committed_generation_ = follower_.generation();
        logiq::utils::Logger::warn("Checkpoint file_id does not match current "
                                   "path inode. Starting from 0.");
      }
    }
  } catch (const std::exception &ex) {
    logiq::utils::Logger::warn(std::string("Failed to load checkpoint: ") +
                               ex.what());
  }

  logiq::utils::Logger::info("Agent initialized.");

  logiq::utils::Logger::info(
      "Batching policy: max_records=" +
      std::to_string(config_.batching.max_records) + " max_bytes=" +
      std::to_string(config_.batching.max_bytes) + " flush_interval_ms=" +
      std::to_string(config_.batching.flush_interval_ms));
  return true;
}

void Agent::run_once() {
  // ---------------------------------------------------------
  // 0) Retry path: do at most one retry attempt per tick
  // ---------------------------------------------------------
  if (auto ready = retry_.pop_ready()) {
    auto res = sink_.send(ready->batch);

    if (res.ok) {
      // Commit only if this batch belongs to the current file identity +
      // generation.
      const auto cur_id = follower_.active_id();
      const auto cur_gen = follower_.generation();

      if (ready->batch.file_dev == cur_id.dev &&
          ready->batch.file_ino == cur_id.ino &&
          ready->batch.file_generation == cur_gen) {

        committed_offset_ =
            std::max(committed_offset_, ready->batch.commit_end_offset);
        committed_generation_ = cur_gen;

        save_checkpoint(cur_id, committed_generation_, committed_offset_);
      }

      retry_.on_success();
    } else {
      ready->last_error = res.message;
      retry_.requeue(std::move(*ready));
    }

    // One unit of work per tick keeps the loop stable (no starvation).
    return;
  }
  // ---------------------------------------------------------
  // 0.5) Time-based flush even if there is no new data
  // ---------------------------------------------------------
  if (auto b = batcher_.flush_if_due()) {
    b->batch_id = next_batch_id();
    auto result = sink_.send(*b);

    if (result.ok) {
      const auto cur_id = follower_.active_id();
      const auto cur_gen = follower_.generation();

      if (b->file_dev == cur_id.dev && b->file_ino == cur_id.ino &&
          b->file_generation == cur_gen) {

        committed_offset_ = std::max(committed_offset_, b->commit_end_offset);
        committed_generation_ = cur_gen;

        save_checkpoint(cur_id, committed_generation_, committed_offset_);
      }
    } else {
      retry_.enqueue(std::move(*b), result.message);
    }

    return; // one unit of work per tick
  }

  // ---------------------------------------------------------
  // 1) Observe filesystem changes (rotation / truncate / missing path)
  // ---------------------------------------------------------
  auto poll = follower_.poll(committed_offset_);

  // Truncate means same inode but size shrank -> new generation and offset must
  // reset.
  if (poll.truncated) {
    framer_.reset();

    // Flush BEFORE overwriting state.
    if (auto b = batcher_.maybe_flush(true)) {
      b->batch_id = next_batch_id();
      auto result = sink_.send(*b);
      if (!result.ok) {
        retry_.enqueue(std::move(*b), result.message);
      }
    }

    committed_offset_ = 0;
    committed_generation_ = follower_.generation();
    save_checkpoint(follower_.active_id(), committed_generation_,
                    committed_offset_);
  }

  // Switching to a new inode at the same path -> start at 0 for the new file.
  if (poll.switched) {
    framer_.reset();

    // Same as truncate: flush with old identity BEFORE resetting state.
    if (auto b = batcher_.maybe_flush(true)) {
      b->batch_id = next_batch_id();
      auto result = sink_.send(*b);
      if (!result.ok) {
        retry_.enqueue(std::move(*b), result.message);
      }
    }

    committed_offset_ = 0;
    committed_generation_ = follower_.generation();
    save_checkpoint(follower_.active_id(), committed_generation_,
                    committed_offset_);
  }

  if (poll.truncated || poll.switched) {
    batcher_.reset(follower_.active_id(), follower_.generation());
  }

  // ---------------------------------------------------------
  // 2) Read new bytes
  // ---------------------------------------------------------
  auto chunk_opt = follower_.read_some();
  if (!chunk_opt)
    return;

  const auto &chunk = *chunk_opt;

  // EOF marker => nothing to do.
  if (chunk.data.empty())
    return;

  // ---------------------------------------------------------
  // 3) Frame into records (line-based framing)
  // ---------------------------------------------------------
  framer_.ingest(chunk.data, chunk.start_offset);
  auto framed = framer_.drain();
  if (framed.empty())
    return;
  // ---------------------------------------------------------
  // 4) Push records into the batcher
  // ---------------------------------------------------------
  for (auto &r : framed) {
    logiq::Record rec;
    rec.payload = r.payload;
    rec.start_offset = r.start_offset;
    rec.end_offset = r.end_offset;

    rec.file_dev = chunk.id.dev;
    rec.file_ino = chunk.id.ino;
    rec.file_generation = chunk.generation;

    // Try to push. If current batch is full, flush and retry.
    if (!batcher_.push(rec)) {
      if (auto b = batcher_.maybe_flush(true)) {
        b->batch_id = next_batch_id();
        auto result = sink_.send(*b);

        if (result.ok) {
          const auto cur_id = follower_.active_id();
          const auto cur_gen = follower_.generation();

          if (b->file_dev == cur_id.dev && b->file_ino == cur_id.ino &&
              b->file_generation == cur_gen) {

            committed_offset_ =
                std::max(committed_offset_, b->commit_end_offset);
            committed_generation_ = cur_gen;

            save_checkpoint(cur_id, committed_generation_, committed_offset_);
          }
        } else {
          retry_.enqueue(std::move(*b), result.message);
        }
      }

      // Now push again (move on retry)
      batcher_.push(std::move(rec));
    } else {
      // If first push used copy, rec is still valid; nothing else.
    }
  }

  // ---------------------------------------------------------
  // 5) Flush if batch limits/time were reached after pushing
  // ---------------------------------------------------------
  if (auto b = batcher_.maybe_flush(false)) {
    b->batch_id = next_batch_id();

    auto result = sink_.send(*b);

    if (result.ok) {
      const auto cur_id = follower_.active_id();
      const auto cur_gen = follower_.generation();

      if (b->file_dev == cur_id.dev && b->file_ino == cur_id.ino &&
          b->file_generation == cur_gen) {

        committed_offset_ = std::max(committed_offset_, b->commit_end_offset);
        committed_generation_ = cur_gen;

        save_checkpoint(cur_id, committed_generation_, committed_offset_);
      }
    } else {
      retry_.enqueue(std::move(*b), result.message);
    }
  }
}

void Agent::shutdown() {
  logiq::utils::Logger::info(
      "Agent shutdown started. Flushing pending data...");

  if (auto b = batcher_.maybe_flush(true)) {
    b->batch_id = next_batch_id();
    logiq::utils::Logger::info("Flushing final batch: " + b->batch_id);

    auto result = sink_.send(*b);

    if (result.ok) {
      const auto cur_id = follower_.active_id();
      const auto cur_gen = follower_.generation();

      if (b->file_dev == cur_id.dev && b->file_ino == cur_id.ino &&
          b->file_generation == cur_gen) {

        committed_offset_ = std::max(committed_offset_, b->commit_end_offset);
        committed_generation_ = cur_gen;

        save_checkpoint(cur_id, committed_generation_, committed_offset_);
        logiq::utils::Logger::info("Final batch flushed and checkpoint saved.");
      }
    } else {
      logiq::utils::Logger::error(
          "Failed to flush final batch during shutdown: " + result.message);
      retry_.enqueue(std::move(*b), result.message);
      // Depending on graceful wait timeouts, you could try to drain the retry
      // queue here. For now we just enqueue it (though it won't be retried
      // unless we save the retry queue to disk later).
    }
  }

  logiq::utils::Logger::info("Agent shutdown complete.");
}

void Agent::save_checkpoint(const logiq::file::FileIdentity &id,
                            std::uint64_t gen, std::uint64_t offset) {
  try {
    logiq::checkpoint::Checkpoint cp;
    cp.file_id = id;
    cp.generation = gen;
    cp.committed_offset = offset;
    checkpoint_store_.save(cp);
  } catch (const std::exception &ex) {
    logiq::utils::Logger::warn(std::string("Failed to persist checkpoint: ") +
                               ex.what());
  }
}

} // namespace logiq::core