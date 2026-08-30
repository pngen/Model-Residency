#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

#include "mr/config.hpp"

namespace mr {

class Uid128 {
 public:
  constexpr Uid128() noexcept : hi_(0), lo_(0) {}

  constexpr Uid128(std::uint64_t hi, std::uint64_t lo) noexcept : hi_(hi), lo_(lo) {}

  static Uid128 from_bytes(const std::array<std::uint8_t, 16>& bytes) noexcept;
  static Uid128 from_bytes(const void* src) noexcept;

  static Uid128 from_hex(std::string_view hex);

  static Uid128 random();

  static constexpr Uid128 nil() noexcept { return Uid128(); }

  [[nodiscard]] constexpr std::uint64_t hi() const noexcept { return hi_; }
  [[nodiscard]] constexpr std::uint64_t lo() const noexcept { return lo_; }

  [[nodiscard]] constexpr bool is_nil() const noexcept { return hi_ == 0 && lo_ == 0; }
  [[nodiscard]] constexpr bool is_zero() const noexcept { return is_nil(); }

  [[nodiscard]] std::array<std::uint8_t, 16> bytes() const noexcept;
  void to_bytes(std::uint8_t out[16]) const noexcept;

  [[nodiscard]] std::string to_hex() const;
  [[nodiscard]] std::string to_canonical() const;

  friend constexpr bool operator==(const Uid128& a, const Uid128& b) noexcept {
    return a.hi_ == b.hi_ && a.lo_ == b.lo_;
  }
  friend constexpr bool operator!=(const Uid128& a, const Uid128& b) noexcept {
    return !(a == b);
  }
  friend constexpr bool operator<(const Uid128& a, const Uid128& b) noexcept {
    return a.hi_ < b.hi_ || (a.hi_ == b.hi_ && a.lo_ < b.lo_);
  }
  friend constexpr bool operator<=(const Uid128& a, const Uid128& b) noexcept {
    return !(b < a);
  }
  friend constexpr bool operator>(const Uid128& a, const Uid128& b) noexcept {
    return b < a;
  }
  friend constexpr bool operator>=(const Uid128& a, const Uid128& b) noexcept {
    return !(a < b);
  }

  friend MR_API std::ostream& operator<<(std::ostream& os, const Uid128& id);

 private:
  std::uint64_t hi_;
  std::uint64_t lo_;
};

} // namespace mr

namespace std {
template <>
struct hash<mr::Uid128> {
  size_t operator()(const mr::Uid128& id) const noexcept {
    std::uint64_t h = id.hi();
    h ^= id.lo() + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return static_cast<size_t>(h);
  }
};
} // namespace std
