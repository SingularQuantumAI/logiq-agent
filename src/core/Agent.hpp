#pragma once

#include "config/Config.hpp"
#include "file/FileFollower.hpp"
#include "framing/LineFramer.hpp"
#include "sinks/HttpNdjsonSink.hpp"

namespace logiq::core {

class Agent {
public:
  explicit Agent(const logiq::config::Config &config);

  bool initialize();
  void run_once();
  void shutdown();

private:
  logiq::config::Config config_;

  logiq::file::FileFollower follower_;
  logiq::framing::LineFramer framer_;
  logiq::sinks::HttpNdjsonSink sink_;

  std::uint64_t committed_offset_{0};
};

} // namespace logiq::core
