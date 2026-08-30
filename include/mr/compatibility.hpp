#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/enums.hpp"
#include "mr/core/identity.hpp"
#include "mr/adapter.hpp"
#include "mr/model.hpp"

namespace mr {

// ===========================================================================
// Compatibility result
// ===========================================================================
/// The result of a typed compatibility check. It is explicit and inspectable:
/// it carries a state and the ordered list of reasons. Compatibility is never
/// inferred from filenames, directory paths, or free-form strings.
class Compatibility {
 public:
  Compatibility() = default;
  explicit Compatibility(CompatibilityState state) : state_(state) {}

  [[nodiscard]] CompatibilityState state() const noexcept { return state_; }
  [[nodiscard]] bool compatible() const noexcept { return state_ == CompatibilityState::Compatible; }
  [[nodiscard]] bool incompatible() const noexcept { return state_ == CompatibilityState::Incompatible; }

  [[nodiscard]] const std::vector<std::string>& reasons() const noexcept { return reasons_; }

  void add_reason(std::string reason) { reasons_.push_back(std::move(reason)); }

  static Compatibility ok() { return Compatibility(CompatibilityState::Compatible); }
  static Compatibility unknown() { return Compatibility(CompatibilityState::Unknown); }

  static Compatibility fail(std::string reason) {
    Compatibility c(CompatibilityState::Incompatible);
    c.add_reason(std::move(reason));
    return c;
  }

 private:
  CompatibilityState state_ = CompatibilityState::Unknown;
  std::vector<std::string> reasons_;
};

// ===========================================================================
// Compatibility checker
// ===========================================================================
/// Deterministic, lock-free typed compatibility validation. Each check returns
/// a structured Compatibility result. All checks are pure functions of their
/// arguments; none can block on I/O or coordination.
class CompatibilityChecker {
 public:
  Compatibility check_model_backend(const ModelRevision& model, BackendType backend) const;
  Compatibility check_model_device(const ModelRevision& model, ComputeCapability cap) const;
  Compatibility check_artifact_generation(const ModelRevision& model, const ArtifactDescriptor& artifact) const;
  Compatibility check_shard_generation(const ShardDescriptor& shard, ArtifactGeneration artifact_generation) const;
  Compatibility check_adapter_base(const Adapter& adapter, const ModelRevision& base) const;
  Compatibility check_adapter_generation(const Adapter& adapter, AdapterGeneration current) const;
  Compatibility check_datatype(DataType expected, DataType actual) const;
  Compatibility check_tokenizer(TokenizerId expected, TokenizerId actual) const;
  Compatibility check_policy_fingerprint(std::string_view expected, std::string_view actual) const;
};

} // namespace mr
