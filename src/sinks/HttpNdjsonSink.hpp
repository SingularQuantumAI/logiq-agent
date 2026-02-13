// File: src/sinks/HttpNdjsonSink.hpp
#pragma once

#include <string>
#include <string_view>

#include "Sink.hpp"

namespace logiq::sinks {

// Minimal HTTP NDJSON sink skeleton.
// This does not implement real HTTP yet; it is a placeholder for wiring +
// tests.
class HttpNdjsonSink final : public logiq::Sink {
public:
  struct Config {
    std::string name{"http"};
    std::string url; // e.g., https://example.com/ingest
    int timeout_ms{2000};
    bool assume_durable_on_200{true}; // if true, treat ok as commit-eligible
  };

  explicit HttpNdjsonSink(Config cfg);

  std::string_view name() const override { return cfg_.name; }
  logiq::SendResult send(const logiq::Batch &batch) noexcept override;

private:
  Config cfg_;

  // Serialize batch records to NDJSON payload.
  // Each record becomes one JSON line. Keep simple for now.
  static std::string to_ndjson(const logiq::Batch &batch);
};

} // namespace logiq::sinks
