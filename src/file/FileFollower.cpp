#include "FileFollower.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace logiq::file {

FileFollower::FileFollower(std::string path)
    : FileFollower(std::move(path), Options{}) {}

FileFollower::FileFollower(std::string path, Options opt)
    : path_(std::move(path)), opt_(opt) {}

std::optional<FileIdentity>
FileFollower::stat_path_id(const std::string &path) {
  struct stat st{};
  if (::stat(path.c_str(), &st) != 0) {
    return std::nullopt;
  }
  return FileIdentity{static_cast<std::uint64_t>(st.st_dev),
                      static_cast<std::uint64_t>(st.st_ino)};
}

std::optional<std::uint64_t> FileFollower::fstat_size(int fd) {
  struct stat st{};
  if (::fstat(fd, &st) != 0) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(st.st_size);
}

bool FileFollower::seek_to(std::uint64_t offset, PollResult &out) {
  off_t rc = ::lseek(fd_, static_cast<off_t>(offset), SEEK_SET);
  if (rc < 0) {
    out.error = true;
    out.message = "lseek failed: " + std::string(std::strerror(errno));
    return false;
  }
  return true;
}

void FileFollower::close_fd(PollResult &out, const std::string &reason) {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
    out.closed = true;
    out.message = reason;
  }
}

bool FileFollower::open_fd_at_path(PollResult &out) {
  auto id = stat_path_id(path_);
  if (!id) {
    // Path doesn't exist (not an error).
    out.path_missing = true;
    return false;
  }

  int fd = ::open(path_.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    // If it exists but we cannot open, that's an error.
    out.error = true;
    out.message = "open failed: " + std::string(std::strerror(errno));
    return false;
  }

  fd_ = fd;
  active_id_ = *id;
  generation_ = 0;
  read_offset_ = 0;
  rotation_pending_ = false;
  last_read_was_eof_ = false;

  out.file_opened = true;
  out.message = "opened file";
  return true;
}

bool FileFollower::open_if_exists() {
  PollResult tmp;
  return open_fd_at_path(tmp);
}

PollResult FileFollower::poll(std::uint64_t committed_offset) {
  PollResult out;

  // If no fd, try to open if path exists.
  if (fd_ < 0) {
    open_fd_at_path(out);
    return out;
  }

  // 1) Detect truncate/copytruncate by comparing current size to our offsets.
  // If size < read_offset => file was truncated while we were reading.
  // Also compare with committed_offset to catch cases where commit > new size.
  if (auto sz = fstat_size(fd_)) {
    if (*sz < read_offset_ ||
        (committed_offset > 0 && *sz < committed_offset)) {
      // Same inode, content shrank. Treat as new generation.
      generation_++;
      read_offset_ = 0;
      (void)seek_to(0, out);
      out.truncated = true;
      out.message = "truncate detected (copytruncate or manual truncate)";
      // Framer should reset on truncate (Agent will do it when it sees
      // out.truncated).
    }
  } else {
    // If fstat fails, fd may be invalid. Close and let it reopen.
    close_fd(out, "fstat failed; closing fd and will reopen");
    return out;
  }

  // 2) Detect rotation by rename/recreate: inode at path changed.
  // Note: If path is missing, old fd might still be valid. Keep draining.
  auto path_id = stat_path_id(path_);
  if (!path_id) {
    out.path_missing = true;

    // If we previously saw EOF and the path is gone, we can close once
    // drained+stable. This prevents holding deleted-but-open files forever.
    if (last_read_was_eof_) {
      auto now = std::chrono::steady_clock::now();
      if (now - last_eof_time_ >= opt_.rotate_settle_time) {
        close_fd(out, "path missing and file drained; closing fd");
      }
    }
    return out;
  }

  // If path points to a different inode than our active fd => rotation pending.
  if (*path_id != active_id_) {
    rotation_pending_ = true;
    pending_id_ = *path_id;
    out.rotated = true;
    out.new_path_id = pending_id_;
    out.message = "rotation detected (path inode changed)";
  }

  // 3) If rotation is pending, maybe switch (only when old file is drained and
  // stable).
  maybe_switch_to_pending(out);

  return out;
}

std::optional<ReadChunk> FileFollower::read_some() {
  if (fd_ < 0)
    return std::nullopt;

  std::string buf;
  buf.resize(opt_.max_read_bytes);

  errno = 0;
  const ssize_t n = ::read(fd_, buf.data(), buf.size());

  if (n > 0) {
    buf.resize(static_cast<std::size_t>(n));

    ReadChunk chunk;
    chunk.start_offset = read_offset_;
    chunk.data = std::move(buf);
    chunk.id = active_id_;
    chunk.generation = generation_;

    read_offset_ += static_cast<std::uint64_t>(n);

    // Not EOF
    last_read_was_eof_ = false;
    return chunk;
  }

  if (n == 0) {
    // EOF right now. This is not final; the writer may append later.
    last_read_was_eof_ = true;
    last_eof_time_ = std::chrono::steady_clock::now();
    return ReadChunk{
        .data = "",
        .start_offset = read_offset_,
        .id = active_id_,
        .generation =
            generation_}; // Empty chunk signals EOF to caller if needed
  }

  // n < 0
  if (errno == EINTR) {
    return ReadChunk{.data = "",
                     .start_offset = read_offset_,
                     .id = active_id_,
                     .generation = generation_};
  }

  // Other read error: close fd and let poll reopen.
  PollResult tmp;
  close_fd(tmp, "read failed; closing fd");
  return std::nullopt;
}

bool FileFollower::maybe_switch_to_pending(PollResult &out) {
  if (!rotation_pending_)
    return false;
  if (fd_ < 0)
    return false;

  // We only switch after we observed EOF and it stayed stable for
  // rotate_settle_time.
  if (!last_read_was_eof_)
    return false;

  auto now = std::chrono::steady_clock::now();
  if (now - last_eof_time_ < opt_.rotate_settle_time) {
    return false;
  }

  // Ensure there is nothing left to read on the old fd.
  // If the old file grew after EOF (writer still flushing), do not switch yet.
  auto sz = fstat_size(fd_);
  if (!sz)
    return false;
  if (*sz > read_offset_) {
    // More data arrived after EOF; keep reading old fd.
    last_read_was_eof_ = false;
    return false;
  }

  // Close old fd.
  if (fd_ >= 0)
    ::close(fd_);
  fd_ = -1;

  // Open the new file currently at path.
  auto current_path_id = stat_path_id(path_);
  if (!current_path_id) {
    // Path disappeared; postpone switching.
    rotation_pending_ = false;
    out.message = "rotation pending but new path missing; will reopen later";
    return false;
  }

  int fd = ::open(path_.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    out.error = true;
    out.message =
        "failed to open rotated file: " + std::string(std::strerror(errno));
    rotation_pending_ = false;
    return false;
  }

  fd_ = fd;
  active_id_ = *current_path_id;
  // New file => reset offsets and generation.
  generation_ = 0;
  read_offset_ = 0;
  last_read_was_eof_ = false;
  rotation_pending_ = false;

  out.switched = true;
  out.message = "switched to rotated file";
  return true;
}

} // namespace logiq::file
