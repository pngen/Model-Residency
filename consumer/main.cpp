#include <iostream>
#include "mr/core/uid.hpp"
#include "mr/core/identity.hpp"
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
  m.required_memory = Bytes(2048); m.required_device_memory = Bytes(2048);
  coord.define_model(std::move(m));
  MemoryDomain dom;
  dom.id = MemoryDomainId::random(); dom.kind = MemoryDomainKind::Accelerator;
  dom.device = DeviceId::random(); dom.backend = BackendType::Cuda; dom.capability = ComputeCapability::Sm_120;
  dom.total_capacity = Bytes::from_mib(4096); dom.governed_capacity = Bytes::from_mib(4096);
  coord.register_domain(std::move(dom));
  AdmitRequest req; req.model = model;
  LoadOutcome o = coord.load(req, AuthorityFrame{});
  std::cout << "downstream find_package consumer: "
            << (o.success ? ("load OK, residency " + o.residency.to_hex()) : ("load FAILED: " + o.error))
            << std::endl;
  return o.success ? 0 : 1;
}
