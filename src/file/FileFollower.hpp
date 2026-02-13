#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "file/FileIdentity.hpp"

namespace logiq::file {

struct ReadChunk {
  std::string data;

  // The file byte-range covered by this chunk is:
  // [start_offset, start_offset + data.size())
  std::uint64_t start_offset{0};

  // Identity of the file that produced this data.
  FileIdentity id{};

  // Increments when we detect truncate/copytruncate on the same inode.
  std::uint64_t generation{0};
};

struct PollResult {
  bool path_missing{false}; // The path does not exist right now.
  bool file_opened{false};  // File was opened in this poll call.
  bool rotated{false};      // Path inode changed (rotation by rename/recreate).
  bool truncated{false};    // File size shrank (copytruncate/truncate).
  bool switched{false};     // We switched from old inode to new inode.
  bool closed{false}; // We closed the active fd (e.g., deleted + drained).
  bool error{false};  // Non-recoverable error encountered.

  std::string message;                     // Debug info
  std::optional<FileIdentity> new_path_id; // Present when rotated detected
};

class FileFollower {
public:
  struct Options {
    std::chrono::milliseconds poll_interval{200};
    std::chrono::milliseconds rotate_settle_time{
        500}; // Wait after EOF before switching
    std::size_t max_read_bytes{64 * 1024};
  };

  explicit FileFollower(std::string path);
  FileFollower(std::string path, Options opt);

  // Attempt to open the file at path. Returns true if opened.
  // If the file doesn't exist yet, returns false (not an error).
  bool open_if_exists();

  // Poll for rotation/truncate/path disappearance.
  // committed_offset is optional but useful to detect edge cases; for MVP you
  // can pass 0.
  PollResult poll(std::uint64_t committed_offset);

  // Read up to max_read_bytes from the active fd.
  // Returns nullopt if no fd open.
  std::optional<ReadChunk> read_some();

  // Exposed state
  bool has_fd() const noexcept { return fd_ >= 0; }
  const std::string &path() const noexcept { return path_; }
  FileIdentity active_id() const noexcept { return active_id_; }
  std::uint64_t generation() const noexcept { return generation_; }
  std::uint64_t read_offset() const noexcept { return read_offset_; }

private:
  std::string path_;
  Options opt_;

  int fd_{-1};
  FileIdentity active_id_{};
  std::uint64_t generation_{0};

  std::uint64_t read_offset_{0};

  // Rotation handling
  bool rotation_pending_{false};
  FileIdentity pending_id_{};

  // EOF tracking for safe switching
  bool last_read_was_eof_{false};
  std::chrono::steady_clock::time_point last_eof_time_{};

private:
  // Open and initialize internal identity/offset.
  bool open_fd_at_path(PollResult &out);

  // Close active fd.
  void close_fd(PollResult &out, const std::string &reason);

  // Get the current identity of the file pointed to by path (stat).
  static std::optional<FileIdentity> stat_path_id(const std::string &path);

  // Get current file size for the open fd (fstat).
  static std::optional<std::uint64_t> fstat_size(int fd);

  // Seek active fd to given offset.
  bool seek_to(std::uint64_t offset, PollResult &out);

  // Attempt to switch to the pending rotated file if old file is drained and
  // stable.
  bool maybe_switch_to_pending(PollResult &out);
};

} // namespace logiq::file
