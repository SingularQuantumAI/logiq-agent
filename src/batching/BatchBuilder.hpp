#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "file/FileIdentity.hpp"
#include "sinks/Sink.hpp" // for logiq::Batch, logiq::Record

namespace logiq::batching {

struct Policy {
  std::uint32_t max_records{500};
  std::uint32_t max_bytes{256 * 1024};
  std::chrono::milliseconds flush_interval{200};
};

class BatchBuilder {
public:
  explicit BatchBuilder(Policy policy);

  // Start a new batch context (file identity + generation).
  // Call on startup and whenever file switched/truncated generation changes.
  void reset(logiq::file::FileIdentity file_id, std::uint64_t generation);

  // Add a record to the current batch.
  // Returns true if added, false if record cannot be added (e.g. oversized
  // record).

  bool push(const logiq::Record &rec); // copia
  bool push(logiq::Record &&rec);      // move

  // Returns a batch if:
  // - size/records thresholds reached (immediate flush), OR
  // - flush interval expired (time-based flush), OR
  // - force == true
  std::optional<logiq::Batch> maybe_flush(bool force);

  // Returns true if there is any pending data in the batch.
  bool has_pending() const noexcept;

  // Convenience: call periodically even if no new logs arrive.
  // Returns a batch if time-based flush is due.
  std::optional<logiq::Batch> flush_if_due();

  // Stats
  std::size_t pending_records() const noexcept { return batch_.records.size(); }
  std::uint64_t pending_bytes() const noexcept { return approx_bytes_; }

private:
  Policy policy_;
  logiq::Batch batch_{};
  std::uint64_t approx_bytes_{0};

  std::chrono::steady_clock::time_point batch_start_{};
  bool started_{false};

private:
  void start_if_needed();
  bool limits_reached() const noexcept;
  static std::uint64_t estimate_record_bytes(const logiq::Record &r);
};

} // namespace logiq::batching