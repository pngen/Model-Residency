#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/byte_size.hpp"
#include "mr/core/enums.hpp"
#include "mr/core/identity.hpp"
#include "mr/explain.hpp"

namespace mr {

// ===========================================================================
// Placement candidate
// ===========================================================================
/// One candidate location considered for a residency decision. Every candidate
/// preserves the constituent factors (not an opaque single score) plus the hard
/// constraint result and the specific reasons for incompatibility, eviction, or
/// migration requirements.
class PlacementCandidate {
 public:
  NodeId node = NodeId::nil();
  DeviceId device = DeviceId::nil();
  MemoryDomainId domain = MemoryDomainId::nil();
  ResidencyClass cls = ResidencyClass::PageableHost;
  MemoryDomainKind domain_kind = MemoryDomainKind::PageableHost;

  bool hard_constraint_ok = false;
  std::vector<std::string> incompatible_reasons; // empty when hard_constraint_ok

  Bytes estimated_movement{0};
  double estimated_load_cost = 0;
  Bytes available_capacity{0};
  double locality = 0;          // 0..1 topology/locality score
  bool existing_warm = false;   // a resident/ready copy already exists here
  ResidencyId warm_residency = ResidencyId::nil();
  bool existing_ready = false;

  bool eviction_required = false;
  std::string eviction_reason; // required eviction candidate description
  bool migration_required = false;

  double expected_benefit = 0;
  double ranking = 0;           // final rank (higher is better)
  bool chosen = false;
  bool final = false;           // is the final chosen location

  Explanation explanation;      // per-candidate factors
};

// ===========================================================================
// Placement decision
// ===========================================================================
/// The outcome of a placement decision: the accepted candidate, the ranked list
/// of alternatives, the deciding factors, and a deterministic tie-break. It
/// never collapses the whole decision into one opaque score.
class PlacementDecision {
 public:
  bool accepted = false;
  bool deferred = false;
  std::string defer_reason;
  PlacementId id = PlacementId::nil();
  std::vector<PlacementCandidate> candidates; // sorted by ranking descending
  std::string tie_break;                      // deterministic tie-break description
  std::vector<ExplanationFactor> deciding_factors;
  std::string summary;

  [[nodiscard]] const PlacementCandidate* chosen() const;
  [[nodiscard]] Json to_json() const;
  [[nodiscard]] std::string to_text() const;
};

} // namespace mr
