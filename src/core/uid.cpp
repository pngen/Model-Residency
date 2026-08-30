#include "mr/core/uid.hpp"

#include "mr/core/error.hpp"

#include <atomic>
#include <cctype>
#include <cstring>
#include <iostream>
#include <random>

namespace mr {
namespace {

// SplitMix64 generator seeded from a mix of random_device and a process-wide
// counter. Even if random_device is deterministic on some platform, the
// unique per-call counter prevents namespace collisions.
class SplitMix64 {
 public:
  SplitMix64(std::uint64_t seed) : state_(seed) {}
  std::uint64_t next() noexcept {
    std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
  }

 private:
  std::uint64_t state_;
};

std::uint64_t next_u64() noexcept {
  static std::atomic<std::uint64_t> counter{0};
  thread_local SplitMix64 gen([] {
    std::random_device rd;
    std::uint64_t seed = (static_cast<std::uint64_t>(rd()) << 32) ^ rd();
    return seed;
  }());
  return gen.next() ^ (static_cast<std::uint64_t>(counter.fetch_add(1, std::memory_order_relaxed)) << 1);
}

constexpr char kHex[] = "0123456789abcdef";

int hex_nibble(char c) noexcept {
  if (c >= '0' && c <= '9') return c - '0';
  const char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (lc >= 'a' && lc <= 'f') return lc - 'a' + 10;
  return -1;
}

} // namespace

Uid128 Uid128::from_bytes(const std::array<std::uint8_t, 16>& bytes) noexcept {
  return from_bytes(bytes.data());
}

Uid128 Uid128::from_bytes(const void* src) noexcept {
  const auto* p = static_cast<const std::uint8_t*>(src);
  std::uint64_t hi = 0, lo = 0;
  for (int i = 0; i < 8; ++i) {
    hi = (hi << 8) | p[i];
    lo = (lo << 8) | p[8 + i];
  }
  return Uid128(hi, lo);
}

Uid128 Uid128::from_hex(std::string_view hex) {
  // Accept the canonical 8-4-4-4-12 form, with or without braces, and any
  // sequence of exactly 32 hex digits optionally separated by '-'.
  std::string compact;
  compact.reserve(32);
  for (char c : hex) {
    if (c == '-' || c == '{' || c == '}') continue;
    throw_if(!std::isxdigit(static_cast<unsigned char>(c)), ErrorCode::InvalidIdentity, "invalid hex digit in Uid128");
    compact.push_back(c);
  }
  throw_if(compact.size() != 32, ErrorCode::InvalidIdentity, "Uid128 hex must be 32 digits");
  std::array<std::uint8_t, 16> bytes{};
  for (int i = 0; i < 16; ++i) {
    const int hi = hex_nibble(compact[2 * i]);
    const int lo = hex_nibble(compact[2 * i + 1]);
    bytes[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((hi << 4) | lo);
  }
  return from_bytes(bytes);
}

Uid128 Uid128::random() { return Uid128(next_u64(), next_u64()); }

std::array<std::uint8_t, 16> Uid128::bytes() const noexcept {
  std::array<std::uint8_t, 16> out{};
  to_bytes(out.data());
  return out;
}

void Uid128::to_bytes(std::uint8_t out[16]) const noexcept {
  for (int i = 0; i < 8; ++i) {
    out[i] = static_cast<std::uint8_t>((hi_ >> (8 * (7 - i))) & 0xFF);
    out[8 + i] = static_cast<std::uint8_t>((lo_ >> (8 * (7 - i))) & 0xFF);
  }
}

std::string Uid128::to_hex() const {
  std::uint8_t raw[16];
  to_bytes(raw);
  std::string s(32, '0');
  for (int i = 0; i < 16; ++i) {
    s[2 * i] = kHex[raw[i] >> 4];
    s[2 * i + 1] = kHex[raw[i] & 0x0F];
  }
  return s;
}

std::string Uid128::to_canonical() const {
  const std::string h = to_hex();
  std::string s;
  s.reserve(36);
  s.append(h, 0, 8); s.push_back('-');
  s.append(h, 8, 4); s.push_back('-');
  s.append(h, 12, 4); s.push_back('-');
  s.append(h, 16, 4); s.push_back('-');
  s.append(h, 20, 12);
  return s;
}

std::ostream& operator<<(std::ostream& os, const Uid128& id) {
  os << id.to_hex();
  return os;
}

} // namespace mr
