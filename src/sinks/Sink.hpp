// File: src/sinks/Sink.hpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace logiq {

using Labels = std::unordered_map<std::string, std::string>;

struct Record {
  // Raw payload (already framed). Keep it as-is; parsing is optional
  // upstream/downstream.
  std::string payload;

  // Deterministic metadata (set by agent).
  std::int64_t ts_ingest_agent_ns{0}; // nanoseconds since epoch
  Labels labels;                      // env, service, host, etc.

  // File identity + byte-range (for checkpointing).
  std::uint64_t file_dev{0};
  std::uint64_t file_ino{0};
  std::uint64_t file_generation{0}; // increments on copytruncate/truncate
  std::uint64_t start_offset{0};
  std::uint64_t end_offset{0}; // exclusive
};

struct Batch {
  std::string batch_id; // unique id (uuid/monotonic)
  std::vector<Record> records;

  // Commit metadata: what can be checkpointed if ACKed.
  std::uint64_t file_dev{0};
  std::uint64_t file_ino{0};
  std::uint64_t file_generation{0};
  std::uint64_t commit_end_offset{
      0};               // highest end_offset in batch for that file/generation
  std::size_t bytes{0}; // approximate payload size
};

struct SendResult {
  bool ok{false};
  int http_status{0};                             // optional for HTTP sinks
  std::string message;                            // error or info
  std::optional<std::uint64_t> commit_end_offset; // if sink confirms durability
};

// A sink is an output backend. Examples: LogControlIQ, OTLP, Kafka, file, etc.
class Sink {
public:
  virtual ~Sink() = default;

  // Unique sink name used by Router rules and logs.
  virtual std::string_view name() const = 0;

  // Send a batch. Must be non-throwing if possible; return ok=false on failure.
  virtual SendResult send(const Batch &batch) noexcept = 0;

  // Optional: allow a sink to report if it's currently "ready".
  virtual bool is_ready() const noexcept { return true; }
};

} // namespace logiq
