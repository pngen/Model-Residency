#include "mr/authority.hpp"

namespace mr {
namespace {

StaleReason gen_axis(std::uint64_t message, std::uint64_t current, StaleReason reason, bool strict) noexcept {
  if (current == 0) return StaleReason::None;         // no authority constraint yet
  if (message == 0) return strict ? reason : StaleReason::None;
  if (message != current) return reason;
  return StaleReason::None;
}

StaleReason id_axis(const Uid128& message, const Uid128& current, StaleReason reason, bool strict) noexcept {
  if (current.is_nil()) return StaleReason::None;
  if (message.is_nil()) return strict ? reason : StaleReason::None;
  if (message != current) return reason;
  return StaleReason::None;
}

} // namespace

const char* stale_reason_name(StaleReason r) noexcept {
  switch (r) {
    case StaleReason::None: return "none";
    case StaleReason::CoordinatorEpoch: return "coordinator-epoch";
    case StaleReason::WorkerBoot: return "worker-boot";
    case StaleReason::ResidencyGeneration: return "residency-generation";
    case StaleReason::ModelGeneration: return "model-generation";
    case StaleReason::ArtifactGeneration: return "artifact-generation";
    case StaleReason::AdapterGeneration: return "adapter-generation";
    case StaleReason::DeviceGeneration: return "device-generation";
    case StaleReason::Attempt: return "attempt";
    case StaleReason::Migration: return "migration";
    case StaleReason::PolicyGeneration: return "policy-generation";
    case StaleReason::Unknown: return "unknown";
  }
  return "unknown";
}

StaleReason StaleAuthorityChecker::validate(const AuthorityFrame& message,
                                            const AuthorityFrame& current,
                                            bool strict_generations) const noexcept {
  // Strict ordering mandated by the spec.
  if (auto r = gen_axis(message.coordinator_epoch.value(), current.coordinator_epoch.value(),
                        StaleReason::CoordinatorEpoch, true); r != StaleReason::None) {
    return r;
  }
  if (auto r = id_axis(message.worker_boot.value(), current.worker_boot.value(),
                       StaleReason::WorkerBoot, true); r != StaleReason::None) {
    return r;
  }
  if (auto r = gen_axis(message.residency_generation.value(), current.residency_generation.value(),
                        StaleReason::ResidencyGeneration, strict_generations); r != StaleReason::None) {
    return r;
  }
  if (auto r = gen_axis(message.model_generation.value(), current.model_generation.value(),
                        StaleReason::ModelGeneration, strict_generations); r != StaleReason::None) {
    return r;
  }
  if (auto r = gen_axis(message.artifact_generation.value(), current.artifact_generation.value(),
                        StaleReason::ArtifactGeneration, strict_generations); r != StaleReason::None) {
    return r;
  }
  if (auto r = gen_axis(message.adapter_generation.value(), current.adapter_generation.value(),
                        StaleReason::AdapterGeneration, strict_generations); r != StaleReason::None) {
    return r;
  }
  if (auto r = gen_axis(message.device_generation.value(), current.device_generation.value(),
                        StaleReason::DeviceGeneration, strict_generations); r != StaleReason::None) {
    return r;
  }
  if (auto r = id_axis(message.attempt.value(), current.attempt.value(),
                       StaleReason::Attempt, false); r != StaleReason::None) {
    return r;
  }
  if (auto r = id_axis(message.migration.value(), current.migration.value(),
                       StaleReason::Migration, false); r != StaleReason::None) {
    return r;
  }
  if (auto r = gen_axis(message.policy_generation.value(), current.policy_generation.value(),
                        StaleReason::PolicyGeneration, strict_generations); r != StaleReason::None) {
    return r;
  }
  return StaleReason::None;
}

} // namespace mr
