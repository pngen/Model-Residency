#include "mr/core/byte_size.hpp"

#include <cmath>

#include "mr/core/error.hpp"

namespace mr {

std::string Bytes::to_string() const {
  const double v = static_cast<double>(value_);
  static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
  int unit = 0;
  double scaled = v;
  while (scaled >= 1024.0 && unit < 5) {
    scaled /= 1024.0;
    ++unit;
  }
  if (unit == 0) {
    return std::to_string(value_) + " B";
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.2f %s", scaled, units[unit]);
  return buf;
}

Bytes checked_add(Bytes a, Bytes b) {
  const std::uint64_t x = a.value();
  const std::uint64_t y = b.value();
  throw_if(x > UINT64_MAX - y, ErrorCode::Overflow, "byte capacity overflow");
  return Bytes(x + y);
}

Bytes checked_sub(Bytes a, Bytes b) {
  const std::uint64_t x = a.value();
  const std::uint64_t y = b.value();
  throw_if(y > x, ErrorCode::Underflow, "byte capacity underflow");
  return Bytes(x - y);
}

std::ostream& operator<<(std::ostream& os, const Bytes& b) {
  os << b.to_string();
  return os;
}

} // namespace mr
