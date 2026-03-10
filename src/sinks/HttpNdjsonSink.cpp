// File: src/sinks/HttpNdjsonSink.cpp
#include "sinks/HttpNdjsonSink.hpp"
#include "utils/Logger.hpp"

#include <curl/curl.h>

#include <sstream>
#include <string>

namespace logiq::sinks {

HttpNdjsonSink::HttpNdjsonSink(Config cfg) : cfg_(std::move(cfg)) {
  curl_ = curl_easy_init();
  if (curl_) {
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-ndjson");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers_ = headers;
  } else {
    logiq::utils::Logger::warn("HttpNdjsonSink: failed to init curl target " +
                               cfg_.url);
  }
}

HttpNdjsonSink::~HttpNdjsonSink() {
  if (headers_) {
    curl_slist_free_all(static_cast<struct curl_slist *>(headers_));
    headers_ = nullptr;
  }
  if (curl_) {
    curl_easy_cleanup(static_cast<CURL *>(curl_));
    curl_ = nullptr;
  }
}

std::string HttpNdjsonSink::to_ndjson(const logiq::Batch &batch) {
  std::ostringstream out;

  // NOTE: Minimal JSON building. Replace with a proper JSON library later.
  for (const auto &r : batch.records) {
    out << "{";
    out << "\"ts_ingest_agent_ns\":" << r.ts_ingest_agent_ns << ",";
    out << "\"payload\":\"";

    // Naive escaping (OK for MVP; upgrade later)
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

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *s = static_cast<std::string *>(userdata);
  const size_t total = size * nmemb;
  s->append(ptr, total);
  return total;
}

logiq::SendResult HttpNdjsonSink::send(const logiq::Batch &batch) noexcept {
  if (cfg_.url.empty()) {
    return {false, 0, "HttpNdjsonSink: url is empty.", std::nullopt};
  }

  if (!curl_) {
    return {false, 0, "HttpNdjsonSink: curl not initialized.", std::nullopt};
  }

  const std::string payload = to_ndjson(batch);
  CURL *curl = static_cast<CURL *>(curl_);
  struct curl_slist *headers = static_cast<struct curl_slist *>(headers_);

  // Reset the handle's state to prevent options from a previous request from
  // leaking into this one. Keep-alive connection pool is still maintained by
  // the handle.
  curl_easy_reset(curl);

  // Enable native TCP keep-alive and force HTTP/1.1 to maximize connection
  // reuse since HTTP/2 multiplexing is not used dynamically yet.
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

  std::string response_body;
  long http_code = 0;

  curl_easy_setopt(curl, CURLOPT_URL, cfg_.url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                   static_cast<long>(payload.size()));

  logiq::utils::Logger::info("HttpNdjsonSink: Sending payload (" +
                             std::to_string(payload.size()) +
                             " bytes): " + payload);

  // Timeouts
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, cfg_.timeout_ms);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, cfg_.connect_timeout_ms);

  // Response capture (useful for debugging)
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

  // Good defaults
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

  const CURLcode rc = curl_easy_perform(curl);
  if (rc == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    logiq::utils::Logger::info("HttpNdjsonSink: Received response (HTTP " +
                               std::to_string(http_code) +
                               "): " + response_body);
  } else {
    logiq::utils::Logger::error("HttpNdjsonSink: curl error: " +
                                std::string(curl_easy_strerror(rc)));
    return {false, 0,
            std::string("HttpNdjsonSink: curl error: ") +
                curl_easy_strerror(rc),
            std::nullopt};
  }

  logiq::SendResult res;
  res.http_status = static_cast<int>(http_code);

  // Treat any 2xx as success
  const bool ok = (http_code >= 200 && http_code < 300);
  res.ok = ok;

  if (ok) {
    res.message = "OK";
    if (cfg_.assume_durable_on_200) {
      res.commit_end_offset = batch.commit_end_offset;
    }
  } else {
    // Include a small response preview for debugging
    std::string preview = response_body;
    if (preview.size() > 300)
      preview.resize(300);
    res.message = "HTTP " + std::to_string(http_code) + " response: " + preview;
  }

  return res;
}

bool HttpNdjsonSink::test_connection() noexcept {
  if (cfg_.url.empty()) {
    return false;
  }

  CURL *test_curl = curl_easy_init();
  if (!test_curl) {
    return false;
  }

  curl_easy_setopt(test_curl, CURLOPT_URL, cfg_.url.c_str());
  curl_easy_setopt(test_curl, CURLOPT_CONNECT_ONLY, 1L);
  curl_easy_setopt(test_curl, CURLOPT_TIMEOUT_MS, cfg_.connect_timeout_ms);

  const CURLcode rc = curl_easy_perform(test_curl);
  curl_easy_cleanup(test_curl);

  return rc == CURLE_OK;
}

} // namespace logiq::sinks