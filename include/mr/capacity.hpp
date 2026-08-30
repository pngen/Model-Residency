#pragma once

#include <string>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/byte_size.hpp"
#include "mr/core/enums.hpp"
#include "mr/core/error.hpp"
#include "mr/core/generation.hpp"
#include "mr/core/identity.hpp"

namespace mr {

// ===========================================================================
// Memory domain counters
// ===========================================================================
/// One managed memory domain (a device, a host pool, or a storage reference).
///
/// Exact accounting invariants are enforced by the guarded mutators below:
///   * reserved + resident <= governed  (no over-allocation)
///   * never go negative                   (no double release / negative accounting)
///   * never overflow / underflow          (checked arithmetic)
///
/// Reserved bytes are temporary holds for in-flight admission attempts; they
/// become resident bytes only when an attempt is committed. This makes failure
/// rollback exact: cancel the reservation and the counters return to baseline
/// with no drift.
class MemoryDomain {
 public:
  MemoryDomainId id = MemoryDomainId::nil();
  MemoryDomainKind kind = MemoryDomainKind::PageableHost;
  DeviceId device = DeviceId::nil(); // nil for host domains
  NodeId node = NodeId::nil();
  BackendType backend = BackendType::Unknown;
  ComputeCapability capability = ComputeCapability::Unknown;
  Bytes total_capacity{0};
  Bytes governed_capacity{0};    // capacity MR may allocate + reserve
  Bytes unavailable_capacity{0}; // hardware that is lost / unavailable
  CapacityGeneration generation = CapacityGeneration::zero();

  // --- accounting state ---
  Bytes reserved_{0};  // in-flight admission holds
  Bytes resident_{0};  // materialized (allocated) bytes owned by MR
  Bytes pinned_{0};    // protected subset of resident_

  // --- predicates ---
  [[nodiscard]] Bytes reserved() const noexcept { return reserved_; }
  [[nodiscard]] Bytes resident() const noexcept { return resident_; }
  [[nodiscard]] Bytes pinned() const noexcept { return pinned_; }
  [[nodiscard]] Bytes reclaimable() const noexcept { return Bytes(resident_.value() - pinned_.value()); }
  [[nodiscard]] Bytes allocated() const noexcept { return resident_; }

  [[nodiscard]] Bytes available() const;

  [[nodiscard]] bool can_reserve(Bytes amount) const;
  [[nodiscard]] bool can_resident(Bytes amount) const;

  /// Reserve capacity for an in-flight attempt. Returns false if the domain
  /// would overcommit; never overcommits.
  bool reserve(Bytes amount);

  /// Commit a previously reserved amount to resident. Throws if the amount
  /// exceeds the outstanding reservation (prevents double commit / negative).
  void commit(Bytes amount);

  /// Cancel a reservation without materializing. Throws on over-release.
  void cancel_reservation(Bytes amount);

  /// Release materialized bytes back to the free pool. Throws on
  /// over-release (prevents double release / negative accounting).
  void release(Bytes amount);

  /// Pin a resident amount so it cannot be reclaimed. Returns false if the
  /// amount exceeds the currently reclaimable resident bytes.
  bool pin(Bytes amount);

  /// Unpin a protected amount, making it reclaimable again. Throws on
  /// over-unpin.
  void unpin(Bytes amount);

  /// Adjust the unavailable capacity (e.g. on device loss).
  void set_unavailable(Bytes amount);

  /// Verify all accounting invariants hold. Throws on any violation.
  void verify_invariants() const;

 private:
  [[nodiscard]] Bytes logically_total() const { return Bytes(governed_capacity.value()); }
};

// ===========================================================================
// Memory domain pool
// ===========================================================================
/// A renderer of active memory domains keyed by identity. The coordinator owns
/// the authoritative set; this helper provides typed lookup and registration.
class MemoryDomainPool {
 public:
  MemoryDomain& register_domain(MemoryDomain domain);
  const MemoryDomain* find(MemoryDomainId id) const;
  MemoryDomain* find(MemoryDomainId id);
  const MemoryDomain* find_device(DeviceId device, MemoryDomainKind kind) const;
  MemoryDomain* find_device(DeviceId device, MemoryDomainKind kind);

  [[nodiscard]] std::vector<MemoryDomainId> ids() const;
  [[nodiscard]] std::size_t size() const noexcept { return domains_.size(); }

 private:
  std::vector<MemoryDomain> domains_;
};

} // namespace mr
