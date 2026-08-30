#pragma once

#include <string>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/byte_size.hpp"
#include "mr/core/error.hpp"
#include "mr/core/generation.hpp"
#include "mr/core/identity.hpp"
#include "mr/model.hpp"

namespace mr {

// ===========================================================================
// Adapter descriptor
// ===========================================================================
/// One adapter (LoRA-style or equivalent) as a first-class object. Adapters
/// bear their own generation and a base-model relationship so a stale adapter
/// generation can never silently attach to a newer base-model generation.
class Adapter {
 public:
  AdapterId id = AdapterId::nil();
  ModelId base_model = ModelId::nil();
  ModelRevisionId base_revision = ModelRevisionId::nil(); // revision the base was built against
  std::string revision;                // informational adapter revision tag
  AdapterGeneration generation = AdapterGeneration::zero();
  std::uint32_t composition_order = 0; // order within an adapter set
  Bytes size{0};
  std::string content_digest;
  ModelFormat format = ModelFormat::Unknown;
  DataType dtype = DataType::Unknown;
  bool active = false;                 // current activation state
  std::string policy_fingerprint;

  [[nodiscard]] bool is_valid() const {
    if (id.is_nil() || base_model.is_nil()) return false;
    if (generation.is_zero()) return false;
    return true;
  }
};

// ===========================================================================
// Adapter set
// ===========================================================================
/// An ordered composition of adapters attached to a base model revision. The
/// set is immutable once assembled; activation/deactivation is tracked at the
/// residency layer, not by mutating the set.
class AdapterSet {
 public:
  AdapterSetId id = AdapterSetId::nil();
  ModelId base_model = ModelId::nil();
  ModelRevisionId base_revision = ModelRevisionId::nil();
  AdapterGeneration generation = AdapterGeneration::zero();
  std::vector<Adapter> adapters;   // in composition order
  Bytes aggregate_size{0};
  std::string content_digest;
  std::vector<BackendType> backend_compat;

  [[nodiscard]] std::size_t count() const noexcept { return adapters.size(); }

  [[nodiscard]] bool is_valid() const {
    if (id.is_nil() || base_model.is_nil()) return false;
    if (generation.is_zero()) return false;
    return true;
  }

  /// Compute aggregate adapter size from member adapters.
  [[nodiscard]] Bytes compute_aggregate_size() const {
    std::uint64_t total = 0;
    for (const auto& a : adapters) { total += a.size.value(); }
    return Bytes(total);
  }
};

} // namespace mr
