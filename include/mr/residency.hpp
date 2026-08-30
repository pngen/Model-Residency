#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/byte_size.hpp"
#include "mr/core/clock.hpp"
#include "mr/core/enums.hpp"
#include "mr/core/error.hpp"
#include "mr/core/generation.hpp"
#include "mr/core/identity.hpp"
#include "mr/model.hpp"

namespace mr {

/// True when a residency class can actually execute a workload. Classes that
/// merely hold an available or loaded representation must never be reported as
/// execution-ready.
MR_API bool residency_class_is_execution_ready(ResidencyClass cls) noexcept;

/// True when the residency class can be demoted to a lower-cost domain or
/// evicted while preserving the artifact identity.
MR_API bool residency_class_is_evictable(ResidencyClass cls) noexcept;

// ===========================================================================
// Lifecycle transition table
// ===========================================================================
/// A residency may only move through the guarded edges below. An invalid
/// transition throws Error(ErrorCode::InvalidTransition, ...). This is the
/// deterministic transition guard required by the lifecycle spec.
MR_API bool lifecycle_transition_allowed(LifecycleState from, LifecycleState to) noexcept;

// ===========================================================================
// Residency
// ===========================================================================
/// One active placement of model-related state on a memory domain.
///
/// A Residency is the unit of governance for the runtime. It distinguishes:
///   * available artifact       (described by ArtifactDescriptor)
///   * loaded representation    (allocated in a domain, class-dependent)
///   * device-resident representation (resident on device memory)
///   * execution-ready representation (declared READY after validation)
class Residency {
 public:
  // --- identity ---
  ResidencyId id = ResidencyId::nil();
  ModelId model = ModelId::nil();
  ModelRevisionId model_revision = ModelRevisionId::nil();
  ArtifactId artifact = ArtifactId::nil();
  ArtifactGeneration artifact_generation = ArtifactGeneration::zero();
  AdapterSetId adapter_set = AdapterSetId::nil();

  // --- placement ---
  NodeId node = NodeId::nil();
  DeviceId device = DeviceId::nil(); // nil for host domains
  MemoryDomainId domain = MemoryDomainId::nil();
  BackendType backend = BackendType::Unknown;
  ComputeCapability capability = ComputeCapability::Unknown;
  ResidencyClass cls = ResidencyClass::PageableHost;
  MemoryDomainKind domain_kind = MemoryDomainKind::PageableHost;

  // --- sizing ---
  Bytes byte_size{0};        // logical footprint of the placement
  Bytes allocated_bytes{0};  // bytes actually accounted in the domain
  Bytes loaded_bytes{0};     // bytes verified loaded

  // --- state ---
  ValidationState validation = ValidationState::Unvalidated;
  ReadinessState readiness = ReadinessState::NotReady;
  CompatibilityState compatibility = CompatibilityState::Unknown;
  LifecycleState lifecycle = LifecycleState::Declared;
  AuthorityState authority = AuthorityState::Unknown;
  ResidencyGeneration generation = ResidencyGeneration::zero();

  // --- provenance / timestamps ---
  std::string source_provenance;
  std::string artifact_path;      // informational, non-authoritative
  MonotonicNs load_time_ns = 0;
  MonotonicNs last_use_ns = 0;
  std::uint64_t usage_count = 0;

  // --- protection / references ---
  std::uint64_t active_references = 0;
  bool pinned = false;

  // --- generation dependencies ---
  ModelGeneration model_generation = ModelGeneration::zero();
  AdapterGeneration adapter_generation = AdapterGeneration::zero();
  DeviceGeneration device_generation = DeviceGeneration::zero();
  PolicyGeneration policy_generation = PolicyGeneration::zero();

  // --- system / migration / eviction ---
  AttemptId attempt = AttemptId::nil();
  MigrationId migration = MigrationId::nil();
  EvictionId eviction = EvictionId::nil();
  EvictionReason eviction_reason = EvictionReason::Manual;
  std::string failed_reason;

  // --- opaque backend handle (NOT serialized; never a reusable live pointer) ---
  void* handle = nullptr;

  // --- lifecycle guard ---
  void transition_to(LifecycleState next);

  /// A residency is resident (loaded into a domain) iff lifecycle is Resident,
  /// Ready, Migrating, Demoting, or Evicting.
  [[nodiscard]] bool is_resident_state() const noexcept;

  /// A residency is execution-ready iff lifecycle == Ready and all state
  /// qualifiers are authoritative and valid, and the class can execute.
  [[nodiscard]] bool is_execution_ready() const noexcept;

  /// Mark the residency as used: bump last-use and usage count.
  void touch() noexcept;

  /// The dependency generation of the named axis.
  [[nodiscard]] std::uint64_t dependency_generation(PlacementFactor f) const noexcept;

 private:
  bool lifecycle_transition_to_ready_guard() const;
};

// ===========================================================================
// Residency set
// ===========================================================================
/// A grouped residency for a model composed of shards, adapters, or mixed
/// components. Partial residency is explicit: the set is complete only when
/// every required member is present and readiness matches.
class ResidencySet {
 public:
  ResidencySetId id = ResidencySetId::nil();
  ModelId model = ModelId::nil();
  ModelRevisionId model_revision = ModelRevisionId::nil();
  AdapterSetId adapter_set = AdapterSetId::nil();
  ResidencyGeneration generation = ResidencyGeneration::zero();

  std::vector<ResidencyId> required_members;
  std::vector<ResidencyId> optional_members;
  std::vector<ResidencyId> members; // ordered membership

  Bytes aggregate_bytes{0};
  bool migration_blocked = false;
  std::string failure_reason;
  MigrationId migration = MigrationId::nil();

  // Coordinator-maintained readiness/completeness (the set is a passive view
  // over coordinated members).
  bool ready = false;
  bool complete = false;

  /// Recompute membership-derived completeness (all required members present).
  [[nodiscard]] bool recompute_complete() const noexcept;

  void set_ready(bool value) noexcept { ready = value; }
  void set_complete(bool value) noexcept { complete = value; }

  [[nodiscard]] bool is_complete() const noexcept { return complete; }
  [[nodiscard]] std::size_t count() const noexcept { return members.size(); }

  /// True when all required members are present (may still be loading).
  [[nodiscard]] bool has_required_members() const noexcept;

  /// True when every required member is execution-ready.
  [[nodiscard]] bool is_ready() const noexcept { return ready; }

  /// True when any member has failed.
  [[nodiscard]] bool has_failure() const noexcept { return !failure_reason.empty(); }
};

} // namespace mr
