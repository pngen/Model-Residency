#pragma once

#include <cstdint>
#include <string>

#include "mr/config.hpp"

namespace mr {

/// A strongly-typed monotonically-increasing generation counter.
///
/// Generations are the backbone of stale-authority rejection: every authority
/// frame carries generations for coordinator epoch, worker boot, residency,
/// model, artifact, adapter, device, and policy. A different Tag produces a
/// distinct type, so a ResidencyGeneration can never be silently compared or
/// assigned to a ModelGeneration.
///
/// Generation values are always represented as the underlying unsigned 64-bit
/// counter. Zero is a valid, explicitly-typed "ground zero" generation. We do
/// not silently recycle identities across generation boundaries.
template <typename Tag>
class Generation {
 public:
  using value_type = std::uint64_t;

  constexpr Generation() noexcept : value_(0) {}
  constexpr explicit Generation(std::uint64_t value) noexcept : value_(value) {}

  /// The zero generation.
  static constexpr Generation zero() noexcept { return Generation(0); }

  /// First generation (1). Use for fresh entities that are not ground zero.
  static constexpr Generation first() noexcept { return Generation(1); }

  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }

  /// The next generation strictly greater than this one.
  [[nodiscard]] constexpr Generation next() const noexcept { return Generation(value_ + 1); }

  [[nodiscard]] constexpr bool is_zero() const noexcept { return value_ == 0; }

  [[nodiscard]] std::string to_hex() const;

  friend constexpr bool operator==(const Generation& a, const Generation& b) noexcept {
    return a.value_ == b.value_;
  }
  friend constexpr bool operator!=(const Generation& a, const Generation& b) noexcept {
    return a.value_ != b.value_;
  }
  friend constexpr bool operator<(const Generation& a, const Generation& b) noexcept {
    return a.value_ < b.value_;
  }
  friend constexpr bool operator<=(const Generation& a, const Generation& b) noexcept {
    return a.value_ <= b.value_;
  }
  friend constexpr bool operator>(const Generation& a, const Generation& b) noexcept {
    return a.value_ > b.value_;
  }
  friend constexpr bool operator>=(const Generation& a, const Generation& b) noexcept {
    return a.value_ >= b.value_;
  }

 private:
  std::uint64_t value_;
};

// Generation domain tags.
struct ResidencyGenerationTag {};
struct ModelGenerationTag {};
struct ArtifactGenerationTag {};
struct AdapterGenerationTag {};
struct DeviceGenerationTag {};
struct PolicyGenerationTag {};
struct DeviceEpochTag {};   // device-generation per device
struct CoordinatorEpochTag {};
struct PlacementGenerationTag {};
struct CapacityGenerationTag {};

using ResidencyGeneration = Generation<ResidencyGenerationTag>;
using ModelGeneration     = Generation<ModelGenerationTag>;
using ArtifactGeneration  = Generation<ArtifactGenerationTag>;
using AdapterGeneration   = Generation<AdapterGenerationTag>;
using DeviceGeneration    = Generation<DeviceGenerationTag>;
using PolicyGeneration    = Generation<PolicyGenerationTag>;
using DeviceEpoch         = Generation<DeviceEpochTag>;
using CoordinatorEpoch    = Generation<CoordinatorEpochTag>;
using PlacementGeneration = Generation<PlacementGenerationTag>;
using CapacityGeneration  = Generation<CapacityGenerationTag>;

} // namespace mr
