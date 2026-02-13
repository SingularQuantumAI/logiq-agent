#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace logiq::framing {

struct FramedRecord {
  std::string payload;
  std::uint64_t start_offset{0};
  std::uint64_t end_offset{0}; // exclusive
};

class LineFramer {
public:
  // Ingest raw bytes from file with base file offset
  void ingest(const std::string &data, std::uint64_t base_offset);

  // Extract completed records
  std::vector<FramedRecord> drain();

  // Reset internal state (used on truncate or rotation)
  void reset();

private:
  std::string buffer_;
  std::uint64_t buffer_start_offset_{0};
};

} // namespace logiq::framing
