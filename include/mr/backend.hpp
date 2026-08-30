#pragma once

#include <string>

#include "mr/config.hpp"
#include "mr/core/byte_size.hpp"
#include "mr/core/enums.hpp"
#include "mr/core/identity.hpp"
#include "mr/adapter.hpp"
#include "mr/model.hpp"

namespace mr {

struct LoadTarget {
  MemoryDomainId domain = MemoryDomainId::nil();
  MemoryDomainKind kind = MemoryDomainKind::PageableHost;
  ResidencyClass cls = ResidencyClass::PageableHost;
  DeviceId device = DeviceId::nil();
  NodeId node = NodeId::nil();
  BackendType backend = BackendType::Unknown;
};

struct LoadResult {
  bool success = false;
  Bytes loaded{0};
  void* handle = nullptr;
  ValidationState validation = ValidationState::Unvalidated;
  std::string provenance;
  std::string error;
};

class ResidencyBackend {
 public:
  virtual ~ResidencyBackend() = default;

  virtual LoadResult load(const LoadTarget& target, const ModelRevision& model,
                          const AdapterSet* adapters) = 0;

  virtual LoadResult migrate(const LoadTarget& from, const LoadTarget& to,
                             const ModelRevision& model, const AdapterSet* adapters,
                             void* source_handle, Bytes bytes) = 0;

  virtual LoadResult verify(const LoadTarget& target, const ModelRevision& model,
                            const AdapterSet* adapters, void* handle) = 0;

  virtual void evict(ResidencyClass cls, void* handle) = 0;
};

} // namespace mr
