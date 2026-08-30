#include "mr/backends/cuda_backend.hpp"

#include <cmath>
#include <cstring>

#include <cuda_runtime.h>

namespace mr {
namespace {

std::string cuda_error(cudaError_t e) {
  return std::string("CUDA: ") + cudaGetErrorString(e);
}

float weight_for(std::uint64_t seed, std::size_t i) {
  std::uint64_t h = seed ^ static_cast<std::uint64_t>(i) * 0x9e3779b97f4a7c15ULL;
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  // Map to a small, finite float in [-1,1].
  const double t = static_cast<double>(h & 0xFFFFFF) / static_cast<double>(0xFFFFFF);
  return static_cast<float>((t * 2.0) - 1.0);
}

__global__ void k_transform(const float* __restrict__ w, float* __restrict__ out, int n) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) { out[i] = w[i] * 3.0f + 0.5f; }
}

} // namespace

std::uint64_t seed_for_model(const ModelRevision& m) {
  std::uint64_t h = 0;
  for (char c : m.revision.to_hex()) { h = (h * 131u) + static_cast<unsigned char>(c); }
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  return h;
}

CudaBackend::CudaBackend() {
  cuda_usable_ = (cudaSetDevice(0) == cudaSuccess);
}

CudaBackend::~CudaBackend() { cuda_usable_ = false; }

std::vector<float> CudaBackend::make_host_weights(const ModelRevision& model) {
  const std::size_t n = static_cast<std::size_t>((model.required_memory.value() + 3u) / 4u);
  const std::uint64_t seed = seed_for_model(model);
  std::vector<float> out(n);
  for (std::size_t i = 0; i < n; ++i) { out[i] = weight_for(seed, i); }
  return out;
}

LoadResult CudaBackend::load(const LoadTarget& target, const ModelRevision& model,
                             const AdapterSet* adapters) {
  (void)target; (void)adapters;
  LoadResult out;
  cudaError_t set = cudaSetDevice(0);
  if (set != cudaSuccess) { cuda_usable_ = false; out.success = false; out.error = cuda_error(set); return out; }
  cuda_usable_ = true;

  const std::size_t bytes = model.required_memory.value();
  const std::size_t n = static_cast<std::size_t>((bytes + 3u) / 4u);
  std::vector<float> host = make_host_weights(model);
  if (host.size() < n) { host.resize(n, 0.0f); }

  CudaBuffer* buf = new CudaBuffer();
  buf->n = n;
  buf->seed = seed_for_model(model);
  cudaError_t all = cudaMalloc(&buf->dev, n * sizeof(float));
  if (all != cudaSuccess) {
    out.success = false; out.error = cuda_error(all);
    delete buf;
    return out;
  }
  cudaError_t cp = cudaMemcpy(buf->dev, host.data(), n * sizeof(float), cudaMemcpyHostToDevice);
  if (cp != cudaSuccess) {
    out.success = false; out.error = cuda_error(cp);
    cudaFree(buf->dev); delete buf;
    return out;
  }
  // Validate device content by reading a few entries back.
  std::vector<float> verify_back(4);
  cudaMemcpy(verify_back.data(), buf->dev, std::min<std::size_t>(4, n) * sizeof(float), cudaMemcpyDeviceToHost);
  bool ok = true;
  for (std::size_t i = 0; i < std::min<std::size_t>(4, n); ++i) {
    if (verify_back[i] != host[i]) { ok = false; break; }
  }
  if (!ok) {
    out.success = false; out.error = "device content verification failed";
    cudaFree(buf->dev); delete buf;
    return out;
  }
  out.success = true;
  out.loaded = Bytes(bytes);
  out.handle = buf;
  out.validation = ValidationState::Valid;
  out.provenance = "cuda-device";
  return out;
}

LoadResult CudaBackend::migrate(const LoadTarget& from, const LoadTarget& to,
                                const ModelRevision& model, const AdapterSet* adapters,
                                void* source_handle, Bytes bytes) {
  (void)from; (void)to; (void)adapters; (void)bytes;
  LoadResult out;
  auto* src = static_cast<CudaBuffer*>(source_handle);
  cudaError_t set = cudaSetDevice(0);
  if (set != cudaSuccess) { out.success = false; out.error = cuda_error(set); return out; }
  CudaBuffer* buf = new CudaBuffer();
  buf->n = src ? src->n : 0;
  buf->seed = src ? src->seed : seed_for_model(model);
  if (buf->n == 0) {
    std::vector<float> host = make_host_weights(model);
    buf->n = static_cast<std::size_t>((model.required_memory.value() + 3u) / 4u);
    if (host.size() < buf->n) { host.resize(buf->n, 0.0f); }
    cudaMalloc(&buf->dev, buf->n * sizeof(float));
    cudaMemcpy(buf->dev, host.data(), buf->n * sizeof(float), cudaMemcpyHostToDevice);
  } else {
    cudaMalloc(&buf->dev, buf->n * sizeof(float));
    cudaMemcpy(buf->dev, src->dev, buf->n * sizeof(float), cudaMemcpyDeviceToDevice);
  }
  out.success = true;
  out.loaded = Bytes(buf->n * sizeof(float));
  out.handle = buf;
  out.validation = ValidationState::Valid;
  return out;
}

LoadResult CudaBackend::verify(const LoadTarget& target, const ModelRevision& model,
                               const AdapterSet* adapters, void* handle) {
  (void)target; (void)model; (void)adapters;
  LoadResult out;
  auto* buf = static_cast<CudaBuffer*>(handle);
  if (buf == nullptr) { out.success = false; out.error = "null handle"; return out; }
  std::vector<float> host = make_host_weights(model);
  if (host.size() < buf->n) { host.resize(buf->n, 0.0f); }
  std::vector<float> back(std::min<std::size_t>(64, buf->n));
  cudaMemcpy(back.data(), buf->dev, back.size() * sizeof(float), cudaMemcpyDeviceToHost);
  bool ok = true;
  for (std::size_t i = 0; i < back.size(); ++i) {
    if (back[i] != host[i]) { ok = false; break; }
  }
  out.success = ok;
  out.loaded = Bytes(buf->n * sizeof(float));
  out.handle = handle;
  out.validation = ok ? ValidationState::Valid : ValidationState::Invalid;
  return out;
}

void CudaBackend::evict(ResidencyClass cls, void* handle) {
  (void)cls;
  if (handle == nullptr) return;
  auto* buf = static_cast<CudaBuffer*>(handle);
  if (buf->dev != nullptr) { cudaFree(buf->dev); }
  delete buf;
}

bool CudaBackend::run_and_verify(void* handle, const std::vector<float>& reference,
                                 double& result, std::string& error) {
  auto* buf = static_cast<CudaBuffer*>(handle);
  if (buf == nullptr) { error = "null handle"; return false; }
  const std::size_t n = buf->n;
  float* out_dev = nullptr;
  cudaError_t all = cudaMalloc(&out_dev, n * sizeof(float));
  if (all != cudaSuccess) { error = cuda_error(all); return false; }
  k_transform<<<(static_cast<unsigned int>((n + 255) / 256)), 256>>>(buf->dev, out_dev, static_cast<int>(n));
  cudaError_t sync = cudaDeviceSynchronize();
  if (sync != cudaSuccess) { error = cuda_error(sync); cudaFree(out_dev); return false; }
  std::vector<float> out_h(n);
  cudaMemcpy(out_h.data(), out_dev, n * sizeof(float), cudaMemcpyDeviceToHost);
  cudaFree(out_dev);
  bool ok = true;
  double sum = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const float expected = reference[i] * 3.0f + 0.5f;
    if (std::fabs(out_h[i] - expected) > 1e-3f) { ok = false; }
    sum += static_cast<double>(out_h[i]);
  }
  result = sum;
  if (!ok) { error = "kernel output mismatched CPU reference"; return false; }
  return true;
}

std::size_t CudaBackend::free_bytes() {
  size_t freeb = 0, totalb = 0;
  cudaMemGetInfo(&freeb, &totalb);
  return freeb;
}

} // namespace mr
