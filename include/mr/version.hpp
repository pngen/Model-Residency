#pragma once

#define MR_VERSION_MAJOR 1
#define MR_VERSION_MINOR 0
#define MR_VERSION_PATCH 0

#define MR_VERSION_STRING "1.0.0"
#define MR_VERSION_MAJOR_INT 1
#define MR_VERSION_MINOR_INT 0
#define MR_VERSION_PATCH_INT 0

namespace mr {

/// Compile-time project version information.
struct Version {
  static constexpr int major = MR_VERSION_MAJOR_INT;
  static constexpr int minor = MR_VERSION_MINOR_INT;
  static constexpr int patch = MR_VERSION_PATCH_INT;

  static constexpr const char* string() noexcept { return MR_VERSION_STRING; }
};

} // namespace mr
