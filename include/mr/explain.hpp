#pragma once

#include <string>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/byte_size.hpp"
#include "mr/core/enums.hpp"
#include "mr/core/json.hpp"

namespace mr {

// ===========================================================================
// Explanation factor
// ===========================================================================
/// One named contribution to a decision. Values carry an AccountingKind so the
/// consumer can tell measured, reported, derived, estimated, and unknown
/// contributions apart — never conflating fabricated with measured.
struct ExplanationFactor {
  std::string key;
  double value = 0;
  double weight = 0;
  std::string note;
  AccountingKind kind = AccountingKind::Derived;
};

// ===========================================================================
// Explanation
// ===========================================================================
/// A structured, inspectable record for a major decision (placement, admission,
/// load, rejection, defer, migration, demotion, eviction, rebalancing,
/// invalidation, reload). Deterministic ordering and JSON emission.
class Explanation {
 public:
  void add_factor(ExplanationFactor f) { factors_.push_back(std::move(f)); }
  void add_narrative(std::string line) { narrative_.push_back(std::move(line)); }

  [[nodiscard]] const std::vector<ExplanationFactor>& factors() const noexcept { return factors_; }
  [[nodiscard]] const std::vector<std::string>& narrative() const noexcept { return narrative_; }

  /// Deterministic JSON rendering.
  [[nodiscard]] Json to_json() const;

  /// Deterministic, human-readable text rendering.
  [[nodiscard]] std::string to_text() const;

  /// Convenience: record a factor computed from a byte count.
  void add_bytes_factor(std::string key, Bytes value, std::string note);

 private:
  std::vector<ExplanationFactor> factors_;
  std::vector<std::string> narrative_;
};

} // namespace mr
