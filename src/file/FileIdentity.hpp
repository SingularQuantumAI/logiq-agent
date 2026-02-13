#pragma once

#include <cstdint>

namespace logiq::file {

struct FileIdentity {
  std::uint64_t dev{0};
  std::uint64_t ino{0};

  bool operator==(const FileIdentity &o) const noexcept {
    return dev == o.dev && ino == o.ino;
  }
  bool operator!=(const FileIdentity &o) const noexcept {
    return !(*this == o);
  }
};

} // namespace logiq::file
