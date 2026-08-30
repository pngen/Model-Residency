#include <iostream>
#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"

int main() {
  using namespace mr;
  MemoryBackend backend;
  Coordinator coord(&backend, ResidencyPolicy{});
  const ModelId model = ModelId::random();
  ModelRevision m;
  m.model = model; m.revision = ModelRevisionId::random(); m.version = "1.0";
  m.artifact.id = ArtifactId::random(); m.artifact.generation = ArtifactGeneration::first();
  m.artifact.backend = BackendType::Cuda; m.artifact.capability = ComputeCapability::Sm_120;
  m.backend_compat = {BackendType::Cuda}; m.device_compat = {ComputeCapability::Sm_120};
  m.required_memory = Bytes(8192); m.required_device_memory = Bytes(8192);
  coord.define_model(std::move(m));
  MemoryDomain dom;
  dom.id = MemoryDomainId::random(); dom.kind = MemoryDomainKind::Accelerator;
  dom.device = DeviceId::random(); dom.backend = BackendType::Cuda; dom.capability = ComputeCapability::Sm_120;
  dom.total_capacity = Bytes::from_mib(1024); dom.governed_capacity = Bytes::from_mib(1024);
  coord.register_domain(std::move(dom));

  AdmitRequest req; req.model = model;
  LoadOutcome o = coord.load(req, AuthorityFrame{});
  if (!o.success) { std::cerr << "load failed: " << o.error << std::endl; return 1; }
  std::cout << "loaded residency " << o.residency.to_hex() << " (" << o.loaded.to_string() << ")" << std::endl;
  const Residency* r = coord.residency(o.residency);
  std::cout << "lifecycle=" << lifecycle_state_name(r->lifecycle)
            << " authority=" << authority_state_name(r->authority) << std::endl;

  LoadOutcome reuse = coord.load(req, AuthorityFrame{});
  std::cout << "second load reused residency: " << (reuse.residency == o.residency ? "yes" : "no") << std::endl;

  EvictRequest ev; ev.residency = o.residency; ev.reason = EvictionReason::Manual;
  EvictionOutcome eo = coord.evict(ev, AuthorityFrame{});
  std::cout << "evicted: " << (eo.success ? "yes" : "no") << std::endl;
  return eo.success ? 0 : 1;
}
