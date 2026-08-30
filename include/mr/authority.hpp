#pragma once

#include <string>

#include "mr/config.hpp"
#include "mr/core/generation.hpp"
#include "mr/core/identity.hpp"

namespace mr {

struct AuthorityFrame {
  CoordinatorEpoch coordinator_epoch = CoordinatorEpoch::zero();
  WorkerBootId worker_boot = WorkerBootId::nil();
  ResidencyGeneration residency_generation = ResidencyGeneration::zero();
  ModelGeneration model_generation = ModelGeneration::zero();
  ArtifactGeneration artifact_generation = ArtifactGeneration::zero();
  AdapterGeneration adapter_generation = AdapterGeneration::zero();
  DeviceGeneration device_generation = DeviceGeneration::zero();
  AttemptId attempt = AttemptId::nil();
  MigrationId migration = MigrationId::nil();
  PolicyGeneration policy_generation = PolicyGeneration::zero();

  bool has_any_generation() const noexcept {
    return !residency_generation.is_zero() || !model_generation.is_zero() ||
           !artifact_generation.is_zero() || !adapter_generation.is_zero() ||
           !device_generation.is_zero();
  }

  static AuthorityFrame production(CoordinatorEpoch epoch) noexcept {
    AuthorityFrame f;
    f.coordinator_epoch = epoch;
    return f;
  }
};

enum class StaleReason {
  None = 0,
  CoordinatorEpoch = 1,
  WorkerBoot = 2,
  ResidencyGeneration = 3,
  ModelGeneration = 4,
  ArtifactGeneration = 5,
  AdapterGeneration = 6,
  DeviceGeneration = 7,
  Attempt = 8,
  Migration = 9,
  PolicyGeneration = 10,
  Unknown = 11,
};

MR_API const char* stale_reason_name(StaleReason r) noexcept;

class StaleAuthorityChecker {
 public:
  [[nodiscard]] StaleReason validate(const AuthorityFrame& message,
                                     const AuthorityFrame& current,
                                     bool strict_generations) const noexcept;
};

} // namespace mr
