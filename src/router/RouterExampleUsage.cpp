// File: src/router/RouterExampleUsage.cpp
// This file is OPTIONAL; it shows how you'd wire Router + Sinks in Agent
// initialization. You can delete it later.
#include "../sinks/HttpNdjsonSink.hpp"
#include "Router.hpp"

#include <iostream>

int main() {
  using namespace logiq;

  router::RouterConfig cfg;
  cfg.ack_policy = router::AckPolicy::Primary;
  cfg.primary_sink_name = "logcontroliq";
  cfg.default_sink_names = {"logcontroliq"};

  router::Router r(cfg);

  auto sink = std::make_shared<sinks::HttpNdjsonSink>(
      sinks::HttpNdjsonSink::Config{.name = "logcontroliq",
                                    .url = "http://127.0.0.1:8080/ingest",
                                    .timeout_ms = 2000,
                                    .assume_durable_on_200 = true});

  r.add_sink(sink);

  std::string error;
  if (!r.validate(error)) {
    std::cerr << "Router validation failed: " << error << "\n";
    return 1;
  }

  Record rec;
  rec.payload = "hello world";
  rec.ts_ingest_agent_ns = 123;

  auto decision = r.decide(rec);

  Batch batch;
  batch.batch_id = "b1";
  batch.records = {rec};
  batch.commit_end_offset = 100;

  std::vector<SendResult> results;
  auto commit = r.send_and_decide_commit(batch, decision, results);

  std::cout << "Commit: " << (commit ? std::to_string(*commit) : "none")
            << "\n";
  return 0;
}
