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
  for (std::uint32_t i = 0; i < 2; ++i) {
    ShardDescriptor s;
    s.id = ShardId::random(); s.index = i; s.total = 2; s.size = Bytes(4096);
    s.generation = ArtifactGeneration::first(); s.required = true;
    m.shards.push_back(s);
  }
  m.required_memory = Bytes(8192);
  coord.define_model(std::move(m));
  MemoryDomain dom;
  dom.id = MemoryDomainId::random(); dom.kind = MemoryDomainKind::Accelerator;
  dom.device = DeviceId::random(); dom.backend = BackendType::Cuda; dom.capability = ComputeCapability::Sm_120;
  dom.total_capacity = Bytes::from_mib(1024); dom.governed_capacity = Bytes::from_mib(1024);
  coord.register_domain(std::move(dom));

  AdmitRequest req; req.model = model;
  LoadOutcome o = coord.load(req, AuthorityFrame{});
  std::cout << "sharded load " << (o.success ? "succeeded" : "failed: " + o.error) << std::endl;
  for (const auto& [sid, set] : coord.sets()) {
    std::cout << "set members=" << set.count() << " required=" << set.required_members.size()
              << " complete=" << set.complete << " ready=" << set.ready << std::endl;
  }
  return o.success ? 0 : 1;
}
