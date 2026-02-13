#include "core/Agent.hpp"
#include "utils/Logger.hpp"

namespace logiq::core {

Agent::Agent(const logiq::config::Config &config)
    : config_(config), follower_(config.input_path),
      sink_({.name = "primary", .url = "http://localhost:8080"}) {}

bool Agent::initialize() {
  follower_.open_if_exists();
  logiq::utils::Logger::info("Agent initialized.");
  return true;
}

void Agent::run_once() {
  // 1️⃣ Observe filesystem changes
  auto poll = follower_.poll(committed_offset_);

  if (poll.truncated || poll.switched) {
    framer_.reset();
  }

  // 2️⃣ Read new data
  auto chunk = follower_.read_some();
  if (!chunk)
    return;

  if (!chunk->data.empty()) {
    framer_.ingest(chunk->data, chunk->start_offset);
  }

  // 3️⃣ Frame into records
  auto records = framer_.drain();
  if (records.empty())
    return;

  // 4️⃣ Build batch
  logiq::Batch batch;
  batch.batch_id = "batch1"; // TODO: real ID
  batch.file_dev = chunk->id.dev;
  batch.file_ino = chunk->id.ino;
  batch.file_generation = chunk->generation;

  for (auto &r : records) {
    logiq::Record rec;
    rec.payload = r.payload;
    rec.start_offset = r.start_offset;
    rec.end_offset = r.end_offset;
    rec.file_dev = chunk->id.dev;
    rec.file_ino = chunk->id.ino;
    rec.file_generation = chunk->generation;
    batch.records.push_back(std::move(rec));
  }

  batch.commit_end_offset = batch.records.back().end_offset;

  // 5️⃣ Send
  auto result = sink_.send(batch);

  // 6️⃣ Commit only if ACK
  if (result.ok) {
    committed_offset_ = batch.commit_end_offset;
    logiq::utils::Logger::debug("Committed offset: " +
                                std::to_string(committed_offset_));
  }
}

void Agent::shutdown() { logiq::utils::Logger::info("Agent shutdown."); }

} // namespace logiq::core
