#pragma once

#include <chrono>
#include <cstdint>

#include "mr/config.hpp"

namespace mr {

/// A nanosecond-precision monotonic time point.
using MonotonicNs = std::int64_t;

/// A nanosecond-precision wall-clock time point (epoch nanoseconds).
using WallNs = std::int64_t;

/// Returns a monotonic timestamp in nanoseconds. Monotonic time cannot jump
/// backwards and is the correct clock to use for duration accounting.
inline MonotonicNs monotonic_ns() noexcept {
  using Clock = std::chrono::steady_clock;
  const auto now = Clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

/// Returns a wall-clock timestamp in nanoseconds since the Unix epoch.
inline WallNs wall_ns() noexcept {
  using Clock = std::chrono::system_clock;
  const auto now = Clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

/// A nanosecond duration used for latency accounting.
using DurationNs = std::int64_t;

inline DurationNs duration_ns_between(MonotonicNs start, MonotonicNs end) noexcept {
  return end - start;
}

} // namespace mr
