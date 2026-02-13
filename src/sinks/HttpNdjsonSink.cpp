// File: src/sinks/HttpNdjsonSink.cpp
#include "HttpNdjsonSink.hpp"

#include <sstream>

namespace logiq::sinks {

HttpNdjsonSink::HttpNdjsonSink(Config cfg) : cfg_(std::move(cfg)) {}

std::string HttpNdjsonSink::to_ndjson(const logiq::Batch &batch) {
  std::ostringstream out;

  // NOTE: This is intentionally minimal and not a full JSON implementation.
  // Replace with a proper JSON library later if needed.
  for (const auto &r : batch.records) {
    out << "{";
    out << "\"ts_ingest_agent_ns\":" << r.ts_ingest_agent_ns << ",";
    out << "\"payload\":\"";

    // Very naive escaping (placeholder). Replace with proper escaping.
    for (char c : r.payload) {
      if (c == '\\' || c == '"')
        out << '\\';
      if (c == '\n') {
        out << "\\n";
        continue;
      }
      if (c == '\r') {
        out << "\\r";
        continue;
      }
      out << c;
    }

    out << "\"";

    if (!r.labels.empty()) {
      out << ",\"labels\":{";
      bool first = true;
      for (const auto &[k, v] : r.labels) {
        if (!first)
          out << ",";
        first = false;

        out << "\"" << k << "\":\"" << v << "\"";
      }
      out << "}";
    }

    out << "}\n";
  }

  return out.str();
}

logiq::SendResult HttpNdjsonSink::send(const logiq::Batch &batch) noexcept {
  // Placeholder logic:
  // - build NDJSON payload
  // - pretend to send it
  // Integrate libcurl (or another HTTP client) later.

  if (cfg_.url.empty()) {
    return {false, 0, "HttpNdjsonSink: url is empty.", std::nullopt};
  }

  const auto payload = to_ndjson(batch);

  // TODO: perform real HTTP POST to cfg_.url with payload (Content-Type:
  // application/x-ndjson). For now, pretend it succeeded.
  (void)payload;

  logiq::SendResult res;
  res.ok = true;
  res.http_status = 200;
  res.message = "OK (stub)";

  // Commit decision:
  // If you trust HTTP 200 means the receiver durably stored the batch, provide
  // commit_end_offset.
  if (cfg_.assume_durable_on_200) {
    res.commit_end_offset = batch.commit_end_offset;
  }

  return res;
}

} // namespace logiq::sinks
