#include "mr/compatibility.hpp"

#include <algorithm>

namespace mr {

Compatibility CompatibilityChecker::check_model_backend(const ModelRevision& model, BackendType backend) const {
  auto owns = [&](BackendType b) {
    return std::find(model.backend_compat.begin(), model.backend_compat.end(), b) != model.backend_compat.end();
  };
  // Explicit backend restriction on the artifact takes precedence.
  if (model.artifact.backend != BackendType::Unknown && model.artifact.backend != backend) {
    return Compatibility::fail(std::string("artifact requires backend ") + backend_type_name(model.artifact.backend) +
                               " but target is " + backend_type_name(backend));
  }
  if (!model.backend_compat.empty() && !owns(backend)) {
    return Compatibility::fail(std::string("model is not compatible with backend ") + backend_type_name(backend));
  }
  return Compatibility::ok();
}

Compatibility CompatibilityChecker::check_model_device(const ModelRevision& model, ComputeCapability cap) const {
  auto owns = [&](ComputeCapability c) {
    return std::find(model.device_compat.begin(), model.device_compat.end(), c) != model.device_compat.end();
  };
  if (model.artifact.capability != ComputeCapability::Unknown && model.artifact.capability != cap) {
    return Compatibility::fail(std::string("artifact requires capability ") +
                               compute_capability_name(model.artifact.capability) + " but target is " +
                               compute_capability_name(cap));
  }
  if (!model.device_compat.empty() && !owns(cap)) {
    return Compatibility::fail(std::string("model is not compatible with capability ") + compute_capability_name(cap));
  }
  return Compatibility::ok();
}

Compatibility CompatibilityChecker::check_artifact_generation(const ModelRevision& model,
                                                              const ArtifactDescriptor& artifact) const {
  if (model.artifact.id != artifact.id) {
    return Compatibility::fail("artifact identity mismatch");
  }
  if (model.artifact.generation == ArtifactGeneration::zero() || artifact.generation.is_zero() ||
      model.artifact.generation != artifact.generation) {
    return Compatibility::fail("artifact generation mismatch");
  }
  return Compatibility::ok();
}

Compatibility CompatibilityChecker::check_shard_generation(const ShardDescriptor& shard,
                                                           ArtifactGeneration artifact_generation) const {
  if (shard.generation.is_zero() || artifact_generation.is_zero() || shard.generation != artifact_generation) {
    return Compatibility::fail("shard generation mismatch");
  }
  return Compatibility::ok();
}

Compatibility CompatibilityChecker::check_adapter_base(const Adapter& adapter, const ModelRevision& base) const {
  if (adapter.base_model != base.model) {
    return Compatibility::fail("adapter base model mismatch");
  }
  if (!adapter.base_revision.is_nil() && adapter.base_revision != base.revision) {
    return Compatibility::fail("adapter base revision mismatch");
  }
  return Compatibility::ok();
}

Compatibility CompatibilityChecker::check_adapter_generation(const Adapter& adapter,
                                                             AdapterGeneration current) const {
  if (adapter.generation.is_zero() || current.is_zero() || adapter.generation != current) {
    return Compatibility::fail("adapter generation mismatch");
  }
  return Compatibility::ok();
}

Compatibility CompatibilityChecker::check_datatype(DataType expected, DataType actual) const {
  if (expected == DataType::Unknown || actual == DataType::Unknown || expected == actual) {
    return Compatibility::ok();
  }
  return Compatibility::fail(std::string("datatype mismatch: ") + data_type_name(expected) + " vs " +
                             data_type_name(actual));
}

Compatibility CompatibilityChecker::check_tokenizer(TokenizerId expected, TokenizerId actual) const {
  if (expected.is_nil() || actual.is_nil() || expected == actual) {
    return Compatibility::ok();
  }
  return Compatibility::fail("tokenizer identity mismatch");
}

Compatibility CompatibilityChecker::check_policy_fingerprint(std::string_view expected,
                                                             std::string_view actual) const {
  if (expected.empty() || actual.empty() || expected == actual) {
    return Compatibility::ok();
  }
  return Compatibility::fail("policy fingerprint mismatch");
}

} // namespace mr
