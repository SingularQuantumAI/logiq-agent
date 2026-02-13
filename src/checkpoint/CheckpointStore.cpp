#include "checkpoint/CheckpointStore.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace logiq::checkpoint {

CheckpointStore::CheckpointStore(std::string path) : path_(std::move(path)) {}

std::optional<Checkpoint> CheckpointStore::load() const {
  if (!fs::exists(path_)) {
    return std::nullopt;
  }

  std::ifstream in(path_, std::ios::in | std::ios::binary);
  if (!in.is_open()) {
    throw std::runtime_error("CheckpointStore: failed to open for read: " +
                             path_);
  }

  std::ostringstream ss;
  ss << in.rdbuf();

  const auto json = ss.str();
  if (json.empty()) {
    throw std::runtime_error("CheckpointStore: checkpoint file is empty: " +
                             path_);
  }

  return from_json(json);
}

void CheckpointStore::save(const Checkpoint &cp) const {
  const fs::path p(path_);
  const fs::path dir = p.parent_path();

  if (!dir.empty()) {
    fs::create_directories(dir);
  }

  const fs::path tmp = p.string() + ".tmp";

  // Write to temp file
  {
    std::ofstream out(tmp, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      throw std::runtime_error(
          "CheckpointStore: failed to open temp file for write: " +
          tmp.string());
    }

    const auto json = to_json(cp);
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    out.flush();

    if (!out.good()) {
      throw std::runtime_error("CheckpointStore: failed writing temp file: " +
                               tmp.string());
    }
  }

  // Atomic replace (POSIX rename)
  std::error_code ec;
  fs::rename(tmp, p, ec);
  if (ec) {
    // If destination exists, try replace strategy
    fs::remove(p, ec);
    ec.clear();
    fs::rename(tmp, p, ec);
    if (ec) {
      throw std::runtime_error("CheckpointStore: rename failed: " +
                               ec.message());
    }
  }
}

std::string CheckpointStore::to_json(const Checkpoint &cp) {
  // Minimal JSON serialization (numbers only). Good enough for MVP.
  std::ostringstream out;
  out << "{";
  out << "\"version\":" << cp.version << ",";
  out << "\"file_dev\":" << cp.file_id.dev << ",";
  out << "\"file_ino\":" << cp.file_id.ino << ",";
  out << "\"generation\":" << cp.generation << ",";
  out << "\"committed_offset\":" << cp.committed_offset;
  out << "}\n";
  return out.str();
}

std::uint64_t CheckpointStore::extract_u64(const std::string &json,
                                           const std::string &key) {
  const std::string needle = "\"" + key + "\":";
  auto pos = json.find(needle);
  if (pos == std::string::npos) {
    throw std::runtime_error("CheckpointStore: missing key: " + key);
  }
  pos += needle.size();

  // Skip whitespace
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                               json[pos] == '\n' || json[pos] == '\r')) {
    pos++;
  }

  // Parse contiguous digits
  std::size_t end = pos;
  while (end < json.size() && (json[end] >= '0' && json[end] <= '9')) {
    end++;
  }

  if (end == pos) {
    throw std::runtime_error("CheckpointStore: invalid number for key: " + key);
  }

  return static_cast<std::uint64_t>(std::stoull(json.substr(pos, end - pos)));
}

Checkpoint CheckpointStore::from_json(const std::string &json) {
  Checkpoint cp;
  cp.version = extract_u64(json, "version");
  cp.file_id.dev = extract_u64(json, "file_dev");
  cp.file_id.ino = extract_u64(json, "file_ino");
  cp.generation = extract_u64(json, "generation");
  cp.committed_offset = extract_u64(json, "committed_offset");
  return cp;
}

} // namespace logiq::checkpoint
