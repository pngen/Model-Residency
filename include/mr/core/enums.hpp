#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "mr/config.hpp"
#include "mr/core/error.hpp"

namespace mr {

// ===========================================================================
// Residency lifecycle
// ===========================================================================
enum class LifecycleState {
  Declared = 0, Planned = 1, Allocating = 2, Loading = 3, Validating = 4,
  Resident = 5, Ready = 6, Migrating = 7, Demoting = 8, Evicting = 9,
  Evicted = 10, Invalidated = 11, Failed = 12, Retired = 13,
};

MR_API const char* lifecycle_state_name(LifecycleState state) noexcept;
MR_API LifecycleState lifecycle_state_from_int(std::int64_t value);

enum class ResidencyClass {
  AcceleratorLocal = 0, PinnedHost = 1, PageableHost = 2, PersistentStorageRef = 3,
  ProcessLocal = 4, SharedHostMapping = 5, GenericBackendMemory = 6,
};

MR_API const char* residency_class_name(ResidencyClass cls) noexcept;
MR_API ResidencyClass residency_class_from_int(std::int64_t value);

enum class MemoryDomainKind {
  Accelerator = 0, PinnedHost = 1, PageableHost = 2, PersistentStorage = 3,
  ProcessLocal = 4, SharedHost = 5, GenericBackend = 6,
};

MR_API const char* memory_domain_kind_name(MemoryDomainKind kind) noexcept;
MR_API MemoryDomainKind memory_domain_kind_from_int(std::int64_t value);

enum class ValidationState {
  Unvalidated = 0, Validating = 1, Valid = 2, Invalid = 3,
};

MR_API const char* validation_state_name(ValidationState s) noexcept;
MR_API ValidationState validation_state_from_int(std::int64_t value);

enum class ReadinessState {
  NotReady = 0, Loading = 1, Ready = 2, Stale = 3, Failed = 4,
};

MR_API const char* readiness_state_name(ReadinessState s) noexcept;
MR_API ReadinessState readiness_state_from_int(std::int64_t value);

enum class CompatibilityState {
  Compatible = 0, Incompatible = 1, Unknown = 2,
};

MR_API const char* compatibility_state_name(CompatibilityState s) noexcept;
MR_API CompatibilityState compatibility_state_from_int(std::int64_t value);

enum class AuthorityState {
  Provisional = 0, Authoritative = 1, Stale = 2, Unknown = 3,
};

MR_API const char* authority_state_name(AuthorityState s) noexcept;
MR_API AuthorityState authority_state_from_int(std::int64_t value);

enum class MemoryPressureState {
  Normal = 0, Elevated = 1, High = 2, Critical = 3,
};

MR_API const char* memory_pressure_state_name(MemoryPressureState s) noexcept;
MR_API MemoryPressureState memory_pressure_state_from_int(std::int64_t value);

enum class AdmissionVerdict {
  Accept = 0, Defer = 1, Reject = 2, RequireEviction = 3,
  RequireMigration = 4, AlternateLocation = 5,
};

MR_API const char* admission_verdict_name(AdmissionVerdict v) noexcept;
MR_API AdmissionVerdict admission_verdict_from_int(std::int64_t value);

enum class MovementAction {
  Keep = 0, Load = 1, Migrate = 2, Demote = 3, Evict = 4, Replace = 5, Invalidate = 6,
};

MR_API const char* movement_action_name(MovementAction a) noexcept;
MR_API MovementAction movement_action_from_int(std::int64_t value);

enum class EvictionReason {
  CapacityPressure = 0, Policy = 1, DeviceLoss = 2, Invalidated = 3,
  Replaced = 4, Administrative = 5, Manual = 6, MigrationComplete = 7,
};

MR_API const char* eviction_reason_name(EvictionReason r) noexcept;
MR_API EvictionReason eviction_reason_from_int(std::int64_t value);

enum class DataType {
  Unknown = 0, Fp32 = 1, Fp16 = 2, Bf16 = 3, Fp8E4M3 = 4, Fp8E5M2 = 5,
  Int8 = 6, Int4 = 7, UInt8 = 8, Int16 = 9, Int32 = 10, Int64 = 11,
  UInt32 = 12, UInt64 = 13, Fp64 = 14,
};

MR_API const char* data_type_name(DataType t) noexcept;
MR_API std::uint64_t data_type_size(DataType t) noexcept;
MR_API DataType data_type_from_int(std::int64_t value);

enum class NumericMode {
  Unknown = 0, Fp16 = 1, Bf16 = 2, Tf32 = 3, Fp32 = 4, Int8 = 5, Int4 = 6, Fp8 = 7, Mixed = 8,
};

MR_API const char* numeric_mode_name(NumericMode m) noexcept;
MR_API NumericMode numeric_mode_from_int(std::int64_t value);

enum class ModelFormat {
  Unknown = 0, SafeTensors = 1, Gguf = 2, PyTorch = 3, Onnx = 4, Ckpt = 5, Custom = 6,
};

MR_API const char* model_format_name(ModelFormat f) noexcept;
MR_API ModelFormat model_format_from_int(std::int64_t value);

enum class BackendType {
  Unknown = 0, Cuda = 1, Rocm = 2, Cpu = 3, Vulkan = 4, Generic = 5,
};

MR_API const char* backend_type_name(BackendType b) noexcept;
MR_API BackendType backend_type_from_int(std::int64_t value);

enum class ComputeCapability {
  Unknown = 0, Sm_70 = 1, Sm_80 = 2, Sm_90 = 3, Sm_100 = 4, Sm_120 = 5, Cpu = 6, Generic = 7,
};

MR_API const char* compute_capability_name(ComputeCapability c) noexcept;
MR_API ComputeCapability compute_capability_from_int(std::int64_t value);

enum class ShardRole {
  Weight = 0, Quantized = 1, Adapter = 2, Tokenizer = 3, Runtime = 4, Auxiliary = 5,
};

MR_API const char* shard_role_name(ShardRole r) noexcept;
MR_API ShardRole shard_role_from_int(std::int64_t value);

enum class AccountingKind {
  Measured = 0, Reported = 1, Derived = 2, Estimated = 3, Unknown = 4,
};

MR_API const char* accounting_kind_name(AccountingKind k) noexcept;
MR_API AccountingKind accounting_kind_from_int(std::int64_t value);

enum class PlacementFactor {
  DeviceCapability = 0, DeviceMemory = 1, HostMemory = 2, ModelSize = 3,
  ShardSizes = 4, AdapterSizes = 5, ExistingResidency = 6, PartialResidency = 7,
  TransferCost = 8, Topology = 9, Locality = 10, ExpectedDemand = 11,
  ActiveReferences = 12, LatencyClass = 13, Priority = 14, TenantConstraints = 15,
  FailureDomain = 16, MemoryPressure = 17, CompetingResidency = 18,
  MigrationCost = 19, EvictionCost = 20, ReloadCost = 21, PolicyGeneration = 22,
};

MR_API const char* placement_factor_name(PlacementFactor f) noexcept;
MR_API PlacementFactor placement_factor_from_int(std::int64_t value);

/// Shared defensive enum-range check. Each *_from_int implementation uses this
/// to reject out-of-range values deterministically.
inline void enum_range_check(std::int64_t value, std::int64_t maxValue, const char* what) {
  if (value < 0 || value > maxValue) {
    throw Error(ErrorCode::InvalidState, std::string(what) + ": invalid enum value");
  }
}

} // namespace mr
