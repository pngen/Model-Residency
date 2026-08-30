#include "mr/residency.hpp"

#include <algorithm>

namespace mr {

bool residency_class_is_execution_ready(ResidencyClass cls) noexcept {
  return cls == ResidencyClass::AcceleratorLocal || cls == ResidencyClass::ProcessLocal;
}

bool residency_class_is_evictable(ResidencyClass cls) noexcept {
  return cls != ResidencyClass::PersistentStorageRef;
}

bool lifecycle_transition_allowed(LifecycleState from, LifecycleState to) noexcept {
  if (from == to) return false;
  switch (from) {
    case LifecycleState::Declared:
      return to == LifecycleState::Planned || to == LifecycleState::Failed;
    case LifecycleState::Planned:
      return to == LifecycleState::Allocating || to == LifecycleState::Failed;
    case LifecycleState::Allocating:
      return to == LifecycleState::Loading || to == LifecycleState::Failed;
    case LifecycleState::Loading:
      return to == LifecycleState::Validating || to == LifecycleState::Failed;
    case LifecycleState::Validating:
      return to == LifecycleState::Resident || to == LifecycleState::Failed;
    case LifecycleState::Resident:
      return to == LifecycleState::Ready || to == LifecycleState::Migrating ||
             to == LifecycleState::Demoting || to == LifecycleState::Evicting ||
             to == LifecycleState::Invalidated || to == LifecycleState::Failed;
    case LifecycleState::Ready:
      return to == LifecycleState::Resident || to == LifecycleState::Migrating ||
             to == LifecycleState::Demoting || to == LifecycleState::Evicting ||
             to == LifecycleState::Invalidated || to == LifecycleState::Retired ||
             to == LifecycleState::Failed;
    case LifecycleState::Migrating:
      return to == LifecycleState::Validating || to == LifecycleState::Resident ||
             to == LifecycleState::Evicted || to == LifecycleState::Retired ||
             to == LifecycleState::Failed;
    case LifecycleState::Demoting:
      return to == LifecycleState::Resident || to == LifecycleState::Evicted ||
             to == LifecycleState::Retired || to == LifecycleState::Failed;
    case LifecycleState::Evicting:
      return to == LifecycleState::Evicted || to == LifecycleState::Failed;
    case LifecycleState::Evicted:
      return to == LifecycleState::Retired;
    case LifecycleState::Invalidated:
      return to == LifecycleState::Retired;
    case LifecycleState::Failed:
      return to == LifecycleState::Retired;
    case LifecycleState::Retired:
      return false;
  }
  return false;
}

void Residency::transition_to(LifecycleState next) {
  throw_unless(lifecycle_transition_allowed(lifecycle, next), ErrorCode::InvalidTransition,
               std::string("invalid lifecycle transition: ") + lifecycle_state_name(lifecycle) + " -> " +
                   lifecycle_state_name(next));
  lifecycle = next;
}

bool Residency::is_resident_state() const noexcept {
  return lifecycle == LifecycleState::Resident || lifecycle == LifecycleState::Ready ||
         lifecycle == LifecycleState::Migrating || lifecycle == LifecycleState::Demoting ||
         lifecycle == LifecycleState::Evicting;
}

bool Residency::lifecycle_transition_to_ready_guard() const {
  return validation == ValidationState::Valid && compatibility == CompatibilityState::Compatible &&
         authority == AuthorityState::Authoritative &&
         residency_class_is_execution_ready(cls);
}

bool Residency::is_execution_ready() const noexcept {
  return lifecycle == LifecycleState::Ready && readiness == ReadinessState::Ready &&
         !generation.is_zero() && lifecycle_transition_to_ready_guard();
}

void Residency::touch() noexcept {
  last_use_ns = monotonic_ns();
  ++usage_count;
}

std::uint64_t Residency::dependency_generation(PlacementFactor) const noexcept {
  // The residency only carries concrete generation members; a caller asking for
  // the dependency generation of a specific axis consults the coordinator. The
  // per-axis generations are exposed through the public members above.
  return 0;
}

bool ResidencySet::recompute_complete() const noexcept {
  bool completeMembers = true;
  for (const auto& req : required_members) {
    if (std::find(members.begin(), members.end(), req) == members.end()) {
      completeMembers = false;
      break;
    }
  }
  return completeMembers;
}

bool ResidencySet::has_required_members() const noexcept {
  return recompute_complete();
}

} // namespace mr
