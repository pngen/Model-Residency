#include "mr/model.hpp"

#include <algorithm>
#include <utility>

#include "mr/core/error.hpp"

namespace mr {

void ModelCatalog::register_revision(ModelRevision revision) {
  throw_unless(revision.is_valid(), ErrorCode::InvalidIdentity,
               "cannot register an invalid model revision");
  // Refuse an older generation silently overwriting a newer one for the same
  // artifact. Generations must be monotonically forward.
  for (const auto& existing : revisions_) {
    if (existing.artifact.id == revision.artifact.id &&
        existing.artifact.generation != revision.artifact.generation) {
      throw_error(ErrorCode::GenerationMismatch,
                  "artifact revision registered with a conflicting generation");
    }
  }
  revisions_.push_back(std::move(revision));
}

const ModelRevision* ModelCatalog::latest_revision(ModelId model) const {
  const ModelRevision* best = nullptr;
  for (const auto& rev : revisions_) {
    if (rev.model != model) continue;
    if (best == nullptr || rev.artifact.generation > best->artifact.generation) {
      best = &rev;
    }
  }
  return best;
}

const ModelRevision* ModelCatalog::find(ModelId model, ModelRevisionId revision) const {
  for (const auto& rev : revisions_) {
    if (rev.model == model && rev.revision == revision) return &rev;
  }
  return nullptr;
}

bool ModelCatalog::has(ModelId model) const { return latest_revision(model) != nullptr; }

std::vector<ModelId> ModelCatalog::model_ids() const {
  std::vector<ModelId> ids;
  for (const auto& rev : revisions_) {
    if (std::find(ids.begin(), ids.end(), rev.model) == ids.end()) ids.push_back(rev.model);
  }
  return ids;
}

} // namespace mr
