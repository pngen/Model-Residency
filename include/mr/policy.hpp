#pragma once

#include <string>

#include "mr/config.hpp"
#include "mr/core/checksum.hpp"
#include "mr/core/enums.hpp"
#include "mr/core/generation.hpp"
#include "mr/core/identity.hpp"
#include "mr/core/strings.hpp"

namespace mr {

// ===========================================================================
// Residency policy
// ===========================================================================
/// Governs the administrative rules that constrain residency movement. A policy
/// carries its own generation so a stale policy never silently governs a fresh
/// residency.
class ResidencyPolicy {
 public:
  PolicyId id = PolicyId::nil();
  PolicyGeneration generation = PolicyGeneration::zero();
  std::string name;

  bool allow_destructive_migration = false; // destroy last source before dest ready
  bool allow_force_eviction = false;        // administrative forced eviction of protected
  bool allow_evict_last_authoritative = true; // free the only authoritative copy when unpinned/unreferenced
  std::uint64_t max_concurrent_migrations = 4;
  double eviction_lru_weight = 1.0;
  double eviction_size_weight = 0.5;
  double eviction_demand_weight = 2.0;
  double load_capacity_margin = 0.05;  // fractional headroom required on target

  /// Deterministic fingerprint of the policy's operative fields.
  [[nodiscard]] std::string fingerprint() const {
    return mr::strprintf("mig=%d force=%d lru=%.4f size=%.4f demand=%.4f margin=%.4f",
                         static_cast<int>(allow_destructive_migration),
                         static_cast<int>(allow_force_eviction),
                         eviction_lru_weight, eviction_size_weight, eviction_demand_weight,
                         load_capacity_margin);
  }
};

} // namespace mr
