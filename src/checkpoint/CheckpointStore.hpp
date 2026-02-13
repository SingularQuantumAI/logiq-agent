#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "file/FileIdentity.hpp"

namespace logiq::checkpoint {

struct Checkpoint {
  // The file identity this checkpoint belongs to.
  logiq::file::FileIdentity file_id{};

  // Increments when we detect copytruncate/truncate on the same inode.
  std::uint64_t generation{0};

  // The last ACKed offset (exclusive). Safe restart position.
  std::uint64_t committed_offset{0};

  // Schema version for forward compatibility.
  std::uint64_t version{1};
};

class CheckpointStore {
public:
  explicit CheckpointStore(std::string path);

  // Load checkpoint from disk. Returns std::nullopt if file does not exist.
  // Throws std::runtime_error on malformed data or IO errors.
  std::optional<Checkpoint> load() const;

  // Save checkpoint to disk (atomic write: temp file + rename).
  // Throws std::runtime_error on IO errors.
  void save(const Checkpoint &cp) const;

  const std::string &path() const noexcept { return path_; }

private:
  std::string path_;

private:
  static std::string to_json(const Checkpoint &cp);
  static Checkpoint from_json(const std::string &json);

  static std::uint64_t extract_u64(const std::string &json,
                                   const std::string &key);
};

} // namespace logiq::checkpoint
