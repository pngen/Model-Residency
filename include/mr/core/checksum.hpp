#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "mr/config.hpp"

namespace mr {

/// CRC-32 (IEEE 802.3 polynomial, reflected) over a byte range.
inline std::uint32_t crc32(const void* data, std::size_t len, std::uint32_t seed = 0) {
  static std::uint32_t table[256];
  static bool init = false;
  if (!init) {
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      }
      table[i] = c;
    }
    init = true;
  }
  const auto* p = static_cast<const std::uint8_t*>(data);
  std::uint32_t crc = ~seed;
  for (std::size_t i = 0; i < len; ++i) {
    crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
  }
  return ~crc;
}

/// FNV-1a 64-bit hash over a byte range.
inline std::uint64_t fnv1a64(const void* data, std::size_t len, std::uint64_t seed = 0xcbf29ce484222325ULL) {
  const auto* p = static_cast<const std::uint8_t*>(data);
  std::uint64_t h = seed;
  constexpr std::uint64_t kPrime = 0x100000001b3ULL;
  for (std::size_t i = 0; i < len; ++i) {
    h ^= p[i];
    h *= kPrime;
  }
  return h;
}

} // namespace mr
