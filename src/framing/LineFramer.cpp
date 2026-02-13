#include "framing/LineFramer.hpp"

namespace logiq::framing {

void LineFramer::ingest(const std::string &data, std::uint64_t base_offset) {
  if (buffer_.empty()) {
    buffer_start_offset_ = base_offset;
  }
  buffer_ += data;
}

std::vector<FramedRecord> LineFramer::drain() {
  std::vector<FramedRecord> out;

  std::size_t pos = 0;
  while (true) {
    auto newline = buffer_.find('\n', pos);
    if (newline == std::string::npos)
      break;

    std::size_t record_len = newline - pos;

    FramedRecord rec;
    rec.payload = buffer_.substr(pos, record_len);
    rec.start_offset = buffer_start_offset_ + pos;
    rec.end_offset = rec.start_offset + record_len + 1; // include '\n'

    out.push_back(std::move(rec));

    pos = newline + 1;
  }

  if (pos > 0) {
    buffer_.erase(0, pos);
    buffer_start_offset_ += pos;
  }

  return out;
}

void LineFramer::reset() {
  buffer_.clear();
  buffer_start_offset_ = 0;
}

} // namespace logiq::framing
