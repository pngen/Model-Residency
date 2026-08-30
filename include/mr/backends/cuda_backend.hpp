#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "mr/config.hpp"
#include "mr/backend.hpp"

namespace mr {

/// A real CUDA (RTX 5090 / sm_120, CUDA 13.1) residency backend.
///
/// It implements the ResidencyBackend interface over actual device memory:
///   * load() allocates a device buffer, transfers deterministic host weights
///     host -> device, and validates the content before reporting success.
///   * verify() runs a real CUDA kernel over the resident state and compares
///     it to a CPU reference, proving the resident copy is executable.
///   * evict() frees the device buffer (cudaFree) so device memory is recovered.
///
/// The handle is a process-local CudaBuffer; it is never serialized and is
/// never valid across a worker or process restart.
class CudaBackend : public ResidencyBackend {
 public:
  CudaBackend();
  ~CudaBackend() override;

  LoadResult load(const LoadTarget& target, const ModelRevision& model,
                  const AdapterSet* adapters) override;
  LoadResult migrate(const LoadTarget& from, const LoadTarget& to,
                     const ModelRevision& model, const AdapterSet* adapters,
                     void* source_handle, Bytes bytes) override;
  LoadResult verify(const LoadTarget& target, const ModelRevision& model,
                    const AdapterSet* adapters, void* handle) override;
  void evict(ResidencyClass cls, void* handle) override;

  /// Deterministic host-side model weight-like buffer (floats).
  static std::vector<float> make_host_weights(const ModelRevision& model);

  /// Run a real CUDA kernel over a resident buffer and compare the output to a
  /// CPU reference. Returns true when the resident state executes correctly.
  static bool run_and_verify(void* handle, const std::vector<float>& reference,
                             double& result, std::string& error);

  /// Current free device bytes (cudaMemGetInfo free).
  static std::size_t free_bytes();

  /// True when a CUDA device is usable by this backend.
  [[nodiscard]] bool usable() const noexcept { return cuda_usable_; }

 private:
  struct CudaBuffer {
    float* dev = nullptr;
    std::size_t n = 0;       // number of floats
    std::uint64_t seed = 0;
  };
  bool cuda_usable_ = false;
};

} // namespace mr
