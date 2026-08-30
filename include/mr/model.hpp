#pragma once

#include <optional>
#include <string>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/byte_size.hpp"
#include "mr/core/enums.hpp"
#include "mr/core/generation.hpp"
#include "mr/core/identity.hpp"

namespace mr {

// ===========================================================================
// Artifact descriptor
// ===========================================================================
/// The identity and characteristics of one persisted model-related artifact.
/// This is the immutable definition side of residency: it describes *what*
/// exists (an available artifact), not *where* it is loaded.
class ArtifactDescriptor {
 public:
  ArtifactId id = ArtifactId::nil();
  ArtifactGeneration generation = ArtifactGeneration::zero();
  ModelFormat format = ModelFormat::Unknown;
  DataType dtype = DataType::Unknown;
  NumericMode mode = NumericMode::Unknown;
  Bytes size{0};
  std::string content_digest;      // hex digest when available
  TokenizerId tokenizer_id = TokenizerId::nil();
  std::string architecture;        // architecture / family, e.g. "llama"
  BackendType backend = BackendType::Unknown;
  ComputeCapability capability = ComputeCapability::Unknown;
  std::string policy_fingerprint;  // policy fingerprint the artifact was built for
  std::optional<std::string> engine; // engine/runtime identity when known
};

// ===========================================================================
// Shard descriptor
// ===========================================================================
class ShardDescriptor {
 public:
  ShardId id = ShardId::nil();
  ShardRole role = ShardRole::Weight;
  std::uint32_t index = 0;      // position in the shard set
  std::uint32_t total = 0;      // total shards in the set
  Bytes size{0};
  std::string content_digest;
  ArtifactGeneration generation = ArtifactGeneration::zero();
  bool required = true;
  std::vector<BackendType> backend_compat; // empty means any backend
};

// ===========================================================================
// Model revision
// ===========================================================================
/// One concrete revision of a model. A model's identity is its ModelId plus a
/// revision plus an artifact generation; the bare name string is never
/// sufficient identity.
class ModelRevision {
 public:
  ModelId model = ModelId::nil();
  ModelRevisionId revision = ModelRevisionId::nil();
  std::string version;                 // human-readable version tag, informational only
  ArtifactDescriptor artifact;
  std::vector<ShardDescriptor> shards; // empty means single monolithic artifact
  Bytes required_memory{0};            // aggregate resident footprint
  Bytes required_device_memory{0};     // accelerator footprint
  std::vector<BackendType> backend_compat;
  std::vector<ComputeCapability> device_compat;
  std::uint32_t max_parallel = 1;

  /// The sum of all required shard sizes. Used for admission accounting.
  [[nodiscard]] Bytes aggregate_shard_size() const {
    std::uint64_t total = 0;
    for (const auto& s : shards) { total += s.size.value(); }
    return Bytes(total);
  }

  /// True when the descriptor carries enough information to be a coherent,
  /// validatable model definition.
  [[nodiscard]] bool is_valid() const {
    if (model.is_nil() || revision.is_nil()) return false;
    if (artifact.id.is_nil()) return false;
    if (artifact.generation.is_zero()) return false;
    if (shards.empty()) return required_memory.value() > 0;
    for (const auto& s : shards) {
      if (s.id.is_nil()) return false;
      if (s.generation.is_zero()) return false;
    }
    return true;
  }
};

// ===========================================================================
// Model catalog
// ===========================================================================
/// Register and resolve model revisions by identity. The catalog is never the
/// source of truth for residency placement; it is the durable definition store.
class ModelCatalog {
 public:
  void register_revision(ModelRevision revision);

  /// Latest (highest-generation) revision for a model.
  [[nodiscard]] const ModelRevision* latest_revision(ModelId model) const;
  [[nodiscard]] const ModelRevision* find(ModelId model, ModelRevisionId revision) const;
  [[nodiscard]] bool has(ModelId model) const;

  [[nodiscard]] std::vector<ModelId> model_ids() const;
  [[nodiscard]] std::size_t size() const noexcept { return revisions_.size(); }

  void clear() noexcept { revisions_.clear(); }

 private:
  std::vector<ModelRevision> revisions_;
};

} // namespace mr
