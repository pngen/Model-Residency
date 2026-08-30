#pragma once

#include <cstdint>
#include <ostream>
#include <string>

#include "mr/config.hpp"

namespace mr {

/// A byte count. It is always non-negative and stored in a 64-bit unsigned.
///
/// Model Residency keeps all capacity accounting in unsigned 64-bit bytes so
/// that overflow, underflow, negative accounting, and silent drift can be
/// detected rather than silently wrapping. Accounting code must call the
/// checked arithmetic helpers below; plain + / - are provided only for
/// convenience in value-that-is-known-not-to-overflow contexts.
class Bytes {
 public:
  constexpr Bytes() noexcept : value_(0) {}
  constexpr explicit Bytes(std::uint64_t value) noexcept : value_(value) {}

  static constexpr Bytes zero() noexcept { return Bytes(0); }
  static constexpr Bytes from_mib(std::uint64_t mib) noexcept { return Bytes(mib * 1024ull * 1024ull); }

  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
  [[nodiscard]] constexpr bool is_zero() const noexcept { return value_ == 0; }

  /// A human-readable rendering such as "1.50 GiB".
  [[nodiscard]] std::string to_string() const;

  friend constexpr bool operator==(const Bytes& a, const Bytes& b) noexcept { return a.value_ == b.value_; }
  friend constexpr bool operator!=(const Bytes& a, const Bytes& b) noexcept { return a.value_ != b.value_; }
  friend constexpr bool operator<(const Bytes& a, const Bytes& b) noexcept { return a.value_ < b.value_; }
  friend constexpr bool operator<=(const Bytes& a, const Bytes& b) noexcept { return a.value_ <= b.value_; }
  friend constexpr bool operator>(const Bytes& a, const Bytes& b) noexcept { return a.value_ > b.value_; }
  friend constexpr bool operator>=(const Bytes& a, const Bytes& b) noexcept { return a.value_ >= b.value_; }

  friend constexpr Bytes operator+(const Bytes& a, const Bytes& b) noexcept { return Bytes(a.value_ + b.value_); }
  friend constexpr Bytes operator-(const Bytes& a, const Bytes& b) noexcept { return Bytes(a.value_ - b.value_); }
  constexpr Bytes& operator+=(const Bytes& b) noexcept { value_ += b.value_; return *this; }
  constexpr Bytes& operator-=(const Bytes& b) noexcept { value_ -= b.value_; return *this; }

  friend MR_API std::ostream& operator<<(std::ostream& os, const Bytes& b);

 private:
  std::uint64_t value_;
};

/// Checked addition that throws on overflow.
MR_API Bytes checked_add(Bytes a, Bytes b);

/// Checked subtraction that throws on underflow (result would be negative).
MR_API Bytes checked_sub(Bytes a, Bytes b);

} // namespace mr
