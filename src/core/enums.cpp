#include "mr/core/enums.hpp"

namespace mr {

#define MR_ENUM_NAME(fn, type, ...) \
  const char* fn(type value) noexcept { \
    switch (value) { \
      __VA_ARGS__ \
    } \
    return "unknown"; \
  }

MR_ENUM_NAME(lifecycle_state_name, LifecycleState,
  case LifecycleState::Declared: return "DECLARED";
  case LifecycleState::Planned: return "PLANNED";
  case LifecycleState::Allocating: return "ALLOCATING";
  case LifecycleState::Loading: return "LOADING";
  case LifecycleState::Validating: return "VALIDATING";
  case LifecycleState::Resident: return "RESIDENT";
  case LifecycleState::Ready: return "READY";
  case LifecycleState::Migrating: return "MIGRATING";
  case LifecycleState::Demoting: return "DEMOTING";
  case LifecycleState::Evicting: return "EVICTING";
  case LifecycleState::Evicted: return "EVICTED";
  case LifecycleState::Invalidated: return "INVALIDATED";
  case LifecycleState::Failed: return "FAILED";
  case LifecycleState::Retired: return "RETIRED";
)

MR_ENUM_NAME(residency_class_name, ResidencyClass,
  case ResidencyClass::AcceleratorLocal: return "accelerator-local";
  case ResidencyClass::PinnedHost: return "pinned-host";
  case ResidencyClass::PageableHost: return "pageable-host";
  case ResidencyClass::PersistentStorageRef: return "persistent-storage-ref";
  case ResidencyClass::ProcessLocal: return "process-local";
  case ResidencyClass::SharedHostMapping: return "shared-host-mapping";
  case ResidencyClass::GenericBackendMemory: return "generic-backend-memory";
)

MR_ENUM_NAME(memory_domain_kind_name, MemoryDomainKind,
  case MemoryDomainKind::Accelerator: return "accelerator";
  case MemoryDomainKind::PinnedHost: return "pinned-host";
  case MemoryDomainKind::PageableHost: return "pageable-host";
  case MemoryDomainKind::PersistentStorage: return "persistent-storage";
  case MemoryDomainKind::ProcessLocal: return "process-local";
  case MemoryDomainKind::SharedHost: return "shared-host";
  case MemoryDomainKind::GenericBackend: return "generic-backend";
)

MR_ENUM_NAME(validation_state_name, ValidationState,
  case ValidationState::Unvalidated: return "unvalidated";
  case ValidationState::Validating: return "validating";
  case ValidationState::Valid: return "valid";
  case ValidationState::Invalid: return "invalid";
)

MR_ENUM_NAME(readiness_state_name, ReadinessState,
  case ReadinessState::NotReady: return "not-ready";
  case ReadinessState::Loading: return "loading";
  case ReadinessState::Ready: return "ready";
  case ReadinessState::Stale: return "stale";
  case ReadinessState::Failed: return "failed";
)

MR_ENUM_NAME(compatibility_state_name, CompatibilityState,
  case CompatibilityState::Compatible: return "compatible";
  case CompatibilityState::Incompatible: return "incompatible";
  case CompatibilityState::Unknown: return "unknown";
)

MR_ENUM_NAME(authority_state_name, AuthorityState,
  case AuthorityState::Provisional: return "provisional";
  case AuthorityState::Authoritative: return "authoritative";
  case AuthorityState::Stale: return "stale";
  case AuthorityState::Unknown: return "unknown";
)

MR_ENUM_NAME(memory_pressure_state_name, MemoryPressureState,
  case MemoryPressureState::Normal: return "normal";
  case MemoryPressureState::Elevated: return "elevated";
  case MemoryPressureState::High: return "high";
  case MemoryPressureState::Critical: return "critical";
)

MR_ENUM_NAME(admission_verdict_name, AdmissionVerdict,
  case AdmissionVerdict::Accept: return "accept";
  case AdmissionVerdict::Defer: return "defer";
  case AdmissionVerdict::Reject: return "reject";
  case AdmissionVerdict::RequireEviction: return "require-eviction";
  case AdmissionVerdict::RequireMigration: return "require-migration";
  case AdmissionVerdict::AlternateLocation: return "alternate-location";
)

MR_ENUM_NAME(movement_action_name, MovementAction,
  case MovementAction::Keep: return "keep";
  case MovementAction::Load: return "load";
  case MovementAction::Migrate: return "migrate";
  case MovementAction::Demote: return "demote";
  case MovementAction::Evict: return "evict";
  case MovementAction::Replace: return "replace";
  case MovementAction::Invalidate: return "invalidate";
)

MR_ENUM_NAME(eviction_reason_name, EvictionReason,
  case EvictionReason::CapacityPressure: return "capacity-pressure";
  case EvictionReason::Policy: return "policy";
  case EvictionReason::DeviceLoss: return "device-loss";
  case EvictionReason::Invalidated: return "invalidated";
  case EvictionReason::Replaced: return "replaced";
  case EvictionReason::Administrative: return "administrative";
  case EvictionReason::Manual: return "manual";
  case EvictionReason::MigrationComplete: return "migration-complete";
)

MR_ENUM_NAME(data_type_name, DataType,
  case DataType::Unknown: return "unknown";
  case DataType::Fp32: return "f32";
  case DataType::Fp16: return "f16";
  case DataType::Bf16: return "bf16";
  case DataType::Fp8E4M3: return "fp8-e4m3";
  case DataType::Fp8E5M2: return "fp8-e5m2";
  case DataType::Int8: return "i8";
  case DataType::Int4: return "i4";
  case DataType::UInt8: return "u8";
  case DataType::Int16: return "i16";
  case DataType::Int32: return "i32";
  case DataType::Int64: return "i64";
  case DataType::UInt32: return "u32";
  case DataType::UInt64: return "u64";
  case DataType::Fp64: return "f64";
)

std::uint64_t data_type_size(DataType t) noexcept {
  switch (t) {
    case DataType::Fp32: return 4;
    case DataType::Fp16: return 2;
    case DataType::Bf16: return 2;
    case DataType::Fp8E4M3:
    case DataType::Fp8E5M2: return 1;
    case DataType::Int8:
    case DataType::UInt8: return 1;
    case DataType::Int4: return 1; // packed, treated as 1 byte per element
    case DataType::Int16: return 2;
    case DataType::Int32:
    case DataType::UInt32: return 4;
    case DataType::Int64:
    case DataType::UInt64: return 8;
    case DataType::Fp64: return 8;
    case DataType::Unknown: return 0;
  }
  return 0;
}

MR_ENUM_NAME(numeric_mode_name, NumericMode,
  case NumericMode::Unknown: return "unknown";
  case NumericMode::Fp16: return "fp16";
  case NumericMode::Bf16: return "bf16";
  case NumericMode::Tf32: return "tf32";
  case NumericMode::Fp32: return "fp32";
  case NumericMode::Int8: return "int8";
  case NumericMode::Int4: return "int4";
  case NumericMode::Fp8: return "fp8";
  case NumericMode::Mixed: return "mixed";
)

MR_ENUM_NAME(model_format_name, ModelFormat,
  case ModelFormat::Unknown: return "unknown";
  case ModelFormat::SafeTensors: return "safetensors";
  case ModelFormat::Gguf: return "gguf";
  case ModelFormat::PyTorch: return "pytorch";
  case ModelFormat::Onnx: return "onnx";
  case ModelFormat::Ckpt: return "ckpt";
  case ModelFormat::Custom: return "custom";
)

MR_ENUM_NAME(backend_type_name, BackendType,
  case BackendType::Unknown: return "unknown";
  case BackendType::Cuda: return "cuda";
  case BackendType::Rocm: return "rocm";
  case BackendType::Cpu: return "cpu";
  case BackendType::Vulkan: return "vulkan";
  case BackendType::Generic: return "generic";
)

MR_ENUM_NAME(compute_capability_name, ComputeCapability,
  case ComputeCapability::Unknown: return "unknown";
  case ComputeCapability::Sm_70: return "sm_70";
  case ComputeCapability::Sm_80: return "sm_80";
  case ComputeCapability::Sm_90: return "sm_90";
  case ComputeCapability::Sm_100: return "sm_100";
  case ComputeCapability::Sm_120: return "sm_120";
  case ComputeCapability::Cpu: return "cpu";
  case ComputeCapability::Generic: return "generic";
)

MR_ENUM_NAME(shard_role_name, ShardRole,
  case ShardRole::Weight: return "weight";
  case ShardRole::Quantized: return "quantized";
  case ShardRole::Adapter: return "adapter";
  case ShardRole::Tokenizer: return "tokenizer";
  case ShardRole::Runtime: return "runtime";
  case ShardRole::Auxiliary: return "auxiliary";
)

MR_ENUM_NAME(accounting_kind_name, AccountingKind,
  case AccountingKind::Measured: return "measured";
  case AccountingKind::Reported: return "reported";
  case AccountingKind::Derived: return "derived";
  case AccountingKind::Estimated: return "estimated";
  case AccountingKind::Unknown: return "unknown";
)

MR_ENUM_NAME(placement_factor_name, PlacementFactor,
  case PlacementFactor::DeviceCapability: return "device-capability";
  case PlacementFactor::DeviceMemory: return "device-memory";
  case PlacementFactor::HostMemory: return "host-memory";
  case PlacementFactor::ModelSize: return "model-size";
  case PlacementFactor::ShardSizes: return "shard-sizes";
  case PlacementFactor::AdapterSizes: return "adapter-sizes";
  case PlacementFactor::ExistingResidency: return "existing-residency";
  case PlacementFactor::PartialResidency: return "partial-residency";
  case PlacementFactor::TransferCost: return "transfer-cost";
  case PlacementFactor::Topology: return "topology";
  case PlacementFactor::Locality: return "locality";
  case PlacementFactor::ExpectedDemand: return "expected-demand";
  case PlacementFactor::ActiveReferences: return "active-references";
  case PlacementFactor::LatencyClass: return "latency-class";
  case PlacementFactor::Priority: return "priority";
  case PlacementFactor::TenantConstraints: return "tenant-constraints";
  case PlacementFactor::FailureDomain: return "failure-domain";
  case PlacementFactor::MemoryPressure: return "memory-pressure";
  case PlacementFactor::CompetingResidency: return "competing-residency";
  case PlacementFactor::MigrationCost: return "migration-cost";
  case PlacementFactor::EvictionCost: return "eviction-cost";
  case PlacementFactor::ReloadCost: return "reload-cost";
  case PlacementFactor::PolicyGeneration: return "policy-generation";
)

#define MR_ENUM_FROM_INT(fn, type, maxValue, what) \
  type fn(std::int64_t value) { \
    enum_range_check(value, maxValue, what); \
    return static_cast<type>(value); \
  }

MR_ENUM_FROM_INT(lifecycle_state_from_int, LifecycleState, 13, "LifecycleState")
MR_ENUM_FROM_INT(residency_class_from_int, ResidencyClass, 6, "ResidencyClass")
MR_ENUM_FROM_INT(memory_domain_kind_from_int, MemoryDomainKind, 6, "MemoryDomainKind")
MR_ENUM_FROM_INT(validation_state_from_int, ValidationState, 3, "ValidationState")
MR_ENUM_FROM_INT(readiness_state_from_int, ReadinessState, 4, "ReadinessState")
MR_ENUM_FROM_INT(compatibility_state_from_int, CompatibilityState, 2, "CompatibilityState")
MR_ENUM_FROM_INT(authority_state_from_int, AuthorityState, 3, "AuthorityState")
MR_ENUM_FROM_INT(memory_pressure_state_from_int, MemoryPressureState, 3, "MemoryPressureState")
MR_ENUM_FROM_INT(admission_verdict_from_int, AdmissionVerdict, 5, "AdmissionVerdict")
MR_ENUM_FROM_INT(movement_action_from_int, MovementAction, 6, "MovementAction")
MR_ENUM_FROM_INT(eviction_reason_from_int, EvictionReason, 7, "EvictionReason")
MR_ENUM_FROM_INT(data_type_from_int, DataType, 14, "DataType")
MR_ENUM_FROM_INT(numeric_mode_from_int, NumericMode, 8, "NumericMode")
MR_ENUM_FROM_INT(model_format_from_int, ModelFormat, 6, "ModelFormat")
MR_ENUM_FROM_INT(backend_type_from_int, BackendType, 5, "BackendType")
MR_ENUM_FROM_INT(compute_capability_from_int, ComputeCapability, 7, "ComputeCapability")
MR_ENUM_FROM_INT(shard_role_from_int, ShardRole, 5, "ShardRole")
MR_ENUM_FROM_INT(accounting_kind_from_int, AccountingKind, 4, "AccountingKind")
MR_ENUM_FROM_INT(placement_factor_from_int, PlacementFactor, 22, "PlacementFactor")

} // namespace mr
