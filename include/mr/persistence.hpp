#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "mr/config.hpp"
#include "mr/coordinator.hpp"

namespace mr {

/// Serialization format magic and version for the versioned, checksummed binary
/// persistence encoding.
inline constexpr std::uint64_t kPersistenceMagic = 0x4D52504552530001ULL; // "MRPER\x00\x01"
inline constexpr std::uint32_t kPersistenceVersion = 1;

// ===========================================================================
// Persistence codec
// ===========================================================================
/// Encodes/decodes a CoordinatorSnapshot to/from a versioned, checksummed
/// binary. On decode every field is validated with a strict bounded reader:
///   * magic and version must match
///   * lengths must not exceed the payload (rejects truncation)
///   * enum values must be in range (rejects invalid enums)
///   * duplicate identifiers are rejected
///   * generation relations must be coherent
///   * the trailing checksum must match (rejects corruption)
///   * NaN/Inf is rejected
/// No live pointers, CUDA handles, device pointers, or contexts are persisted;
/// on recovery live residency must be revalidated before readiness is granted.
class PersistenceCodec {
 public:
  [[nodiscard]] static std::vector<std::uint8_t> encode(const CoordinatorSnapshot& snapshot);
  static CoordinatorSnapshot decode(std::span<const std::uint8_t> bytes);
  [[nodiscard]] static std::span<const std::uint8_t> read_length_checked(std::span<const std::uint8_t> data,
                                                                          std::size_t offset, std::size_t length);
};

} // namespace mr
