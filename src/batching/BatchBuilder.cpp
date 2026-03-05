#include "batching/BatchBuilder.hpp"

#include <algorithm>
#include <utility>

namespace logiq::batching {

BatchBuilder::BatchBuilder(Policy policy) : policy_(std::move(policy)) {}

void BatchBuilder::reset(logiq::file::FileIdentity file_id,
                         std::uint64_t generation) {
  batch_ = logiq::Batch{};
  batch_.batch_id.clear(); // set by Agent (or later by a UUID generator)
  batch_.file_dev = file_id.dev;
  batch_.file_ino = file_id.ino;
  batch_.file_generation = generation;
  batch_.commit_end_offset = 0;

  approx_bytes_ = 0;
  started_ = false;
}

bool BatchBuilder::has_pending() const noexcept {
  return !batch_.records.empty();
}

void BatchBuilder::start_if_needed() {
  if (!started_) {
    batch_start_ = std::chrono::steady_clock::now();
    started_ = true;
  }
}

std::uint64_t BatchBuilder::estimate_record_bytes(const logiq::Record &r) {
  // Approximate size: payload + small overhead for JSON/NDJSON framing.
  // (This is NOT exact, but sufficient for controlling batch sizes.)
  return static_cast<std::uint64_t>(r.payload.size()) + 64;
}

bool BatchBuilder::push(const logiq::Record &rec) {
  logiq::Record tmp = rec;
  return push(std::move(tmp));
}

bool BatchBuilder::push(logiq::Record &&rec) {
  const auto rec_bytes = estimate_record_bytes(rec);

  // Enforce strict max_records (optional but recommended)
  if (!batch_.records.empty() && policy_.max_records > 0 &&
      batch_.records.size() >= policy_.max_records) {
    return false;
  }

  // Enforce strict max_bytes (optional but recommended)
  if (!batch_.records.empty() && policy_.max_bytes > 0 &&
      (approx_bytes_ + rec_bytes) > policy_.max_bytes) {
    return false;
  }

  // Allow single oversize record only when batch is empty
  if (batch_.records.empty() && policy_.max_bytes > 0 &&
      rec_bytes > policy_.max_bytes) {
    // allowed as single-record batch
  }

  start_if_needed();
  approx_bytes_ += rec_bytes;

  batch_.commit_end_offset =
      std::max<std::uint64_t>(batch_.commit_end_offset, rec.end_offset);
  batch_.records.push_back(std::move(rec));
  return true;
}

bool BatchBuilder::limits_reached() const noexcept {
  if (policy_.max_records > 0 && batch_.records.size() >= policy_.max_records)
    return true;
  if (policy_.max_bytes > 0 && approx_bytes_ >= policy_.max_bytes)
    return true;
  return false;
}

std::optional<logiq::Batch> BatchBuilder::maybe_flush(bool force) {
  if (batch_.records.empty())
    return std::nullopt;

  const auto now = std::chrono::steady_clock::now();
  const bool due_by_time =
      started_ && (now - batch_start_ >= policy_.flush_interval);

  if (!force && !limits_reached() && !due_by_time) {
    return std::nullopt;
  }

  // Move out the batch
  logiq::Batch out = std::move(batch_);

  // Reset internal batch but keep same file context
  const auto dev = out.file_dev;
  const auto ino = out.file_ino;
  const auto gen = out.file_generation;

  reset(logiq::file::FileIdentity{dev, ino}, gen);
  return out;
}

std::optional<logiq::Batch> BatchBuilder::flush_if_due() {
  return maybe_flush(false);
}

} // namespace logiq::batching