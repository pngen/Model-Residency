#pragma once

#include <cstdint>
#include <vector>

#include "mr/config.hpp"
#include "mr/backend.hpp"

namespace mr {

/// A deterministic, in-memory residency backend. It simulates the byte
/// movement and content verification of a real deployment without any hardware
/// dependency. It is the default backend for tests, examples, and the
/// coordinator/worker in-process path; the CUDA backend implements the same
/// interface over real device memory.
///
/// Handle semantics:
///   * load() allocates a new buffer and fills it with a deterministic pattern
///     derived from the model revision id + any adapter id.
///   * verify() recomputes the pattern and compares against the loaded bytes.
///   * migrate() copies the source bytes into a fresh buffer (the source is not
///     freed; the coordinator decides when to retire it).
///   * evict() frees the buffer and is idempotent for a null handle.
class MemoryBackend : public ResidencyBackend {
 public:
  LoadResult load(const LoadTarget& target, const ModelRevision& model,
                  const AdapterSet* adapters) override;
  LoadResult migrate(const LoadTarget& from, const LoadTarget& to,
                     const ModelRevision& model, const AdapterSet* adapters,
                     void* source_handle, Bytes bytes) override;
  LoadResult verify(const LoadTarget& target, const ModelRevision& model,
                    const AdapterSet* adapters, void* handle) override;
  void evict(ResidencyClass cls, void* handle) override;

  /// The current count of live buffers (for leak checks in tests).
  [[nodiscard]] std::size_t live_buffers() const noexcept { return live_; }

 private:
  struct Buffer {
    std::vector<std::uint8_t> data;
    std::uint64_t seed = 0;
  };

  static std::uint64_t seed_for(const ModelRevision& model, const AdapterSet* adapters);
  static void fill(Buffer& buf) noexcept;
  static bool verify_buffer(const Buffer& buf) noexcept;

  void* allocate(Buffer* buf);

  std::size_t live_ = 0;
};

} // namespace mr
