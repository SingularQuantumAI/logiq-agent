// File: src/router/Router.cpp
#include "Router.hpp"

namespace logiq::router {

Router::Router(RouterConfig cfg) : cfg_(std::move(cfg)) {}

void Router::add_sink(std::shared_ptr<logiq::Sink> sink) {
  if (!sink)
    return;
  sinks_by_name_[std::string(sink->name())] = std::move(sink);
}

bool Router::validate(std::string &error) const {
  if (cfg_.default_sink_names.empty() && cfg_.rules.empty()) {
    error = "RouterConfig: no default sinks and no rules configured.";
    return false;
  }

  if (cfg_.ack_policy == AckPolicy::Primary) {
    if (cfg_.primary_sink_name.empty()) {
      error =
          "RouterConfig: primary_sink_name is required for AckPolicy::Primary.";
      return false;
    }
    if (!get_sink_ptr(cfg_.primary_sink_name)) {
      error = "RouterConfig: primary sink not found: " + cfg_.primary_sink_name;
      return false;
    }
  }

  // Validate referenced sinks exist (best-effort check).
  for (const auto &s : cfg_.default_sink_names) {
    if (!get_sink_ptr(s)) {
      error = "RouterConfig: default sink not found: " + s;
      return false;
    }
  }
  for (const auto &rule : cfg_.rules) {
    for (const auto &s : rule.sink_names) {
      if (!get_sink_ptr(s)) {
        error = "RouterConfig: rule references unknown sink: " + s;
        return false;
      }
    }
  }

  return true;
}

logiq::Sink *Router::get_sink_ptr(std::string_view name) const noexcept {
  auto it = sinks_by_name_.find(std::string(name));
  if (it == sinks_by_name_.end())
    return nullptr;
  return it->second.get();
}

bool Router::rule_matches(const RouteRule &rule,
                          const logiq::Record &record) const {
  auto it = record.labels.find(rule.label_key);
  if (it == record.labels.end())
    return false;
  return it->second == rule.label_value;
}

RouteDecision Router::decide(const logiq::Record &record) const {
  RouteDecision decision;

  // First-match rule routing (simple + deterministic).
  for (const auto &rule : cfg_.rules) {
    if (!rule_matches(rule, record))
      continue;

    for (const auto &sink_name : rule.sink_names) {
      if (auto *s = get_sink_ptr(sink_name)) {
        decision.sinks.push_back(s);
        if (cfg_.ack_policy == AckPolicy::Primary &&
            sink_name == cfg_.primary_sink_name) {
          decision.uses_primary = true;
        }
      }
    }
    return decision;
  }

  // Fallback: default sinks.
  for (const auto &sink_name : cfg_.default_sink_names) {
    if (auto *s = get_sink_ptr(sink_name)) {
      decision.sinks.push_back(s);
      if (cfg_.ack_policy == AckPolicy::Primary &&
          sink_name == cfg_.primary_sink_name) {
        decision.uses_primary = true;
      }
    }
  }

  return decision;
}

std::optional<std::uint64_t> Router::send_and_decide_commit(
    const logiq::Batch &batch, const RouteDecision &decision,
    std::vector<logiq::SendResult> &per_sink_results) noexcept {
  per_sink_results.clear();
  per_sink_results.reserve(decision.sinks.size());

  if (decision.sinks.empty()) {
    per_sink_results.push_back(
        {false, 0, "No sinks selected by router.", std::nullopt});
    return std::nullopt;
  }

  bool any_ok = false;
  bool all_ok = true;

  bool primary_ok = false;
  std::optional<std::uint64_t> primary_commit;

  std::optional<std::uint64_t> any_commit;

  for (auto *sink : decision.sinks) {
    if (!sink || !sink->is_ready()) {
      per_sink_results.push_back(
          {false, 0, "Sink not ready or null.", std::nullopt});
      all_ok = false;
      continue;
    }

    auto res = sink->send(batch);
    if (res.ok) {
      any_ok = true;
      any_commit = res.commit_end_offset.has_value() ? res.commit_end_offset
                                                     : any_commit;
    } else {
      all_ok = false;
    }

    // Primary tracking (by name comparison).
    if (cfg_.ack_policy == AckPolicy::Primary &&
        sink->name() == cfg_.primary_sink_name) {
      primary_ok = res.ok;
      primary_commit = res.commit_end_offset;
    }

    per_sink_results.push_back(std::move(res));
  }

  // Decide commit according to policy.
  switch (cfg_.ack_policy) {
  case AckPolicy::Primary:
    if (!primary_ok)
      return std::nullopt;
    // If sink doesn't return a commit offset, fall back to batch's
    // commit_end_offset only if you trust HTTP 200 implies durability. Keep
    // conservative by preferring explicit.
    return primary_commit.has_value()
               ? primary_commit
               : std::optional<std::uint64_t>{batch.commit_end_offset};

  case AckPolicy::Any:
    if (!any_ok)
      return std::nullopt;
    return any_commit.has_value()
               ? any_commit
               : std::optional<std::uint64_t>{batch.commit_end_offset};

  case AckPolicy::All:
    if (!all_ok)
      return std::nullopt;
    // If all OK, commit at batch end. (Assumes all sinks durable on success.)
    return std::optional<std::uint64_t>{batch.commit_end_offset};
  }

  return std::nullopt;
}

} // namespace logiq::router
