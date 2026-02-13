// File: src/router/Router.hpp
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../sinks/Sink.hpp"

namespace logiq::router {

enum class AckPolicy {
  // Commit when the primary sink ACKs (recommended default).
  Primary,

  // Commit when any sink ACKs (best-effort reliability).
  Any,

  // Commit only when all routed sinks ACK (strongest, but can stall).
  All
};

struct RouteRule {
  // Minimal rule: route by label equals value.
  // Extend later with regex, severity, service, etc.
  std::string label_key;
  std::string label_value;

  // If matched, send to these sinks (by name).
  std::vector<std::string> sink_names;
};

struct RouterConfig {
  AckPolicy ack_policy{AckPolicy::Primary};
  std::string primary_sink_name; // required for Primary policy

  // Default sinks used if no rule matches.
  std::vector<std::string> default_sink_names;

  // Optional routing rules.
  std::vector<RouteRule> rules;
};

struct RouteDecision {
  std::vector<logiq::Sink *> sinks; // resolved sink pointers
  bool uses_primary{false};         // whether primary is among sinks
};

// Router is responsible for selecting sinks and managing send/ack decisions.
// It does not own checkpoints; it returns commit info for Agent to persist.
class Router {
public:
  explicit Router(RouterConfig cfg);

  // Add a sink. Router keeps the pointer alive via shared_ptr.
  void add_sink(std::shared_ptr<logiq::Sink> sink);

  // Validate configuration (e.g., primary sink exists).
  // Returns false with message if invalid.
  [[nodiscard]] bool validate(std::string &error) const;

  // Decide which sinks to use for a given record.
  [[nodiscard]] RouteDecision decide(const logiq::Record &record) const;

  // Send a batch to the sinks selected for the batch.
  // Returns the effective commit_end_offset according to AckPolicy.
  // If commit is not possible (no ACK condition satisfied), returns nullopt.
  [[nodiscard]] std::optional<std::uint64_t> send_and_decide_commit(
      const logiq::Batch &batch, const RouteDecision &decision,
      std::vector<logiq::SendResult> &per_sink_results) noexcept;

  const RouterConfig &config() const noexcept { return cfg_; }

private:
  RouterConfig cfg_;
  std::unordered_map<std::string, std::shared_ptr<logiq::Sink>> sinks_by_name_;

  [[nodiscard]] logiq::Sink *get_sink_ptr(std::string_view name) const noexcept;
  [[nodiscard]] bool rule_matches(const RouteRule &rule,
                                  const logiq::Record &record) const;
};

} // namespace logiq::router
