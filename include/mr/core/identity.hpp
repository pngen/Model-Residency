#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "mr/config.hpp"
#include "mr/core/uid.hpp"

namespace mr {

/// A strongly-typed 128-bit identity.
///
/// Each entity domain declares a distinct Tag type. The resulting Id<Tag> types
/// are unrelated to each other so a ModelId can never be silently assigned to a
/// NodeId or an AdapterId. This is the backbone of "never silently reuse
/// identity across authority generations."
template <typename Tag>
class Id {
 public:
  constexpr Id() noexcept : value_() {}
  constexpr explicit Id(Uid128 value) noexcept : value_(value) {}

  [[nodiscard]] static Id nil() noexcept { return Id(Uid128::nil()); }
  [[nodiscard]] static Id random() noexcept { return Id(Uid128::random()); }

  [[nodiscard]] static Id from_hex(std::string_view hex) { return Id(Uid128::from_hex(hex)); }
  [[nodiscard]] static Id from_bytes(const std::array<std::uint8_t, 16>& bytes) {
    return Id(Uid128::from_bytes(bytes));
  }

  [[nodiscard]] constexpr const Uid128& value() const noexcept { return value_; }

  [[nodiscard]] constexpr bool is_nil() const noexcept { return value_.is_nil(); }
  [[nodiscard]] constexpr bool is_zero() const noexcept { return value_.is_nil(); }

  [[nodiscard]] std::string to_hex() const { return value_.to_hex(); }
  [[nodiscard]] std::string to_canonical() const { return value_.to_canonical(); }

  friend constexpr bool operator==(const Id& a, const Id& b) noexcept { return a.value_ == b.value_; }
  friend constexpr bool operator!=(const Id& a, const Id& b) noexcept { return a.value_ != b.value_; }
  friend constexpr bool operator<(const Id& a, const Id& b) noexcept { return a.value_ < b.value_; }
  friend constexpr bool operator<=(const Id& a, const Id& b) noexcept { return a.value_ <= b.value_; }
  friend constexpr bool operator>(const Id& a, const Id& b) noexcept { return a.value_ > b.value_; }
  friend constexpr bool operator>=(const Id& a, const Id& b) noexcept { return a.value_ >= b.value_; }

 private:
  Uid128 value_;
};

// Identity domain tags. One per entity domain so the wrappers are distinct.
struct ModelIdTag {};
struct ModelRevisionIdTag {};
struct ArtifactIdTag {};
struct ShardIdTag {};
struct AdapterIdTag {};
struct AdapterSetIdTag {};
struct ResidencyIdTag {};
struct ResidencySetIdTag {};
struct PlacementIdTag {};
struct TenantIdTag {};
struct WorkloadIdTag {};
struct NodeIdTag {};
struct DeviceIdTag {};
struct WorkerIdTag {};
struct WorkerBootIdTag {};
struct AttemptIdTag {};
struct MigrationIdTag {};
struct EvictionIdTag {};
struct PolicyIdTag {};
struct MemoryDomainIdTag {};
struct BackendIdTag {};
struct TokenizerIdTag {};
struct DatasetIdTag {};

using ModelId       = Id<ModelIdTag>;
using ModelRevisionId = Id<ModelRevisionIdTag>;
using ArtifactId    = Id<ArtifactIdTag>;
using ShardId       = Id<ShardIdTag>;
using AdapterId     = Id<AdapterIdTag>;
using AdapterSetId  = Id<AdapterSetIdTag>;
using ResidencyId   = Id<ResidencyIdTag>;
using ResidencySetId = Id<ResidencySetIdTag>;
using PlacementId   = Id<PlacementIdTag>;
using TenantId      = Id<TenantIdTag>;
using WorkloadId    = Id<WorkloadIdTag>;
using NodeId        = Id<NodeIdTag>;
using DeviceId      = Id<DeviceIdTag>;
using WorkerId      = Id<WorkerIdTag>;
using WorkerBootId  = Id<WorkerBootIdTag>;
using AttemptId     = Id<AttemptIdTag>;
using MigrationId   = Id<MigrationIdTag>;
using EvictionId    = Id<EvictionIdTag>;
using PolicyId      = Id<PolicyIdTag>;
using MemoryDomainId = Id<MemoryDomainIdTag>;
using BackendId     = Id<BackendIdTag>;
using TokenizerId   = Id<TokenizerIdTag>;
using DatasetId     = Id<DatasetIdTag>;

} // namespace mr

namespace std {
template <typename Tag>
struct hash<mr::Id<Tag>> {
  size_t operator()(const mr::Id<Tag>& id) const noexcept {
    return std::hash<mr::Uid128>{}(id.value());
  }
};
} // namespace std
