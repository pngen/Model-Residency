#include "mr/backends/memory_backend.hpp"

#include <atomic>
#include <cstddef>
#include <cstring>
#include <functional>

#include "mr/core/checksum.hpp"
#include "mr/core/error.hpp"

namespace mr {
namespace {

std::uint64_t mixed_seed(std::uint64_t a, std::uint64_t b) {
  std::uint64_t h = a;
  h ^= b + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  return h;
}

std::uint8_t pattern(std::uint64_t seed, std::size_t i) {
  std::uint64_t h = seed ^ static_cast<std::uint64_t>(i) * 0x9e3779b97f4a7c15ULL;
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  return static_cast<std::uint8_t>(h & 0xFF);
}

} // namespace

std::uint64_t MemoryBackend::seed_for(const ModelRevision& model, const AdapterSet* adapters) {
  std::uint64_t seed = mixed_seed(std::hash<std::string>{}(model.revision.to_hex()),
                                  std::hash<std::string>{}(model.artifact.content_digest));
  if (adapters != nullptr && !adapters->adapters.empty()) {
    seed = mixed_seed(seed, std::hash<std::string>{}(adapters->id.to_hex()));
  }
  return seed;
}

void MemoryBackend::fill(Buffer& buf) noexcept {
  const std::uint64_t seed = buf.seed;
  for (std::size_t i = 0; i < buf.data.size(); ++i) {
    buf.data[i] = pattern(seed, i);
  }
}

bool MemoryBackend::verify_buffer(const Buffer& buf) noexcept {
  const std::uint64_t seed = buf.seed;
  for (std::size_t i = 0; i < buf.data.size(); ++i) {
    if (buf.data[i] != pattern(seed, i)) return false;
  }
  return true;
}

void* MemoryBackend::allocate(Buffer* buf) {
  auto* p = new Buffer(std::move(*buf));
  ++live_;
  return p;
}

LoadResult MemoryBackend::load(const LoadTarget& target, const ModelRevision& model,
                               const AdapterSet* adapters) {
  (void)target;
  LoadResult out;
  if (model.required_memory.is_zero() && model.shards.empty()) {
    out.success = false;
    out.error = "model requires zero bytes";
    return out;
  }
  Buffer buf;
  buf.seed = seed_for(model, adapters);
  const std::uint64_t n = model.required_memory.value();
  buf.data.resize(n);
  fill(buf);
  out.success = true;
  out.loaded = Bytes(n);
  out.handle = allocate(&buf);
  out.validation = ValidationState::Valid;
  out.provenance = "memory";
  return out;
}

LoadResult MemoryBackend::migrate(const LoadTarget& from, const LoadTarget& to,
                                  const ModelRevision& model, const AdapterSet* adapters,
                                  void* source_handle, Bytes bytes) {
  (void)from;
  (void)to;
  (void)bytes;
  LoadResult out;
  auto* src = static_cast<Buffer*>(source_handle);
  Buffer buf;
  if (src != nullptr) {
    buf.seed = src->seed;
    buf.data = src->data;
  } else {
    buf.seed = seed_for(model, adapters);
    buf.data.resize(model.required_memory.value());
    fill(buf);
  }
  out.success = true;
  out.loaded = Bytes(buf.data.size());
  out.handle = allocate(&buf);
  out.validation = ValidationState::Valid;
  return out;
}

LoadResult MemoryBackend::verify(const LoadTarget& target, const ModelRevision& model,
                                 const AdapterSet* adapters, void* handle) {
  (void)target;
  (void)model;
  (void)adapters;
  LoadResult out;
  auto* buf = static_cast<Buffer*>(handle);
  if (buf == nullptr) {
    out.success = false;
    out.error = "null handle";
    return out;
  }
  out.success = verify_buffer(*buf);
  out.loaded = Bytes(buf->data.size());
  out.handle = handle;
  out.validation = out.success ? ValidationState::Valid : ValidationState::Invalid;
  return out;
}

void MemoryBackend::evict(ResidencyClass cls, void* handle) {
  (void)cls;
  if (handle == nullptr) return;
  delete static_cast<Buffer*>(handle);
  if (live_ > 0) { --live_; }
}

} // namespace mr
