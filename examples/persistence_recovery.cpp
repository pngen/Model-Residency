#include <cstdio>
#include <iostream>
#include <string>
#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"

int main() {
  using namespace mr;
  const std::string path = "mr_example_state.bin";
  {
    MemoryBackend backend;
    Coordinator coord(&backend, ResidencyPolicy{});
    const ModelId model = ModelId::random();
    ModelRevision m;
    m.model = model; m.revision = ModelRevisionId::random(); m.version = "1.0";
    m.artifact.id = ArtifactId::random(); m.artifact.generation = ArtifactGeneration::first();
    m.artifact.backend = BackendType::Cuda; m.artifact.capability = ComputeCapability::Sm_120;
    m.backend_compat = {BackendType::Cuda}; m.device_compat = {ComputeCapability::Sm_120};
    m.required_memory = Bytes(4096); m.required_device_memory = Bytes(4096);
    coord.define_model(std::move(m));
    MemoryDomain dom;
    dom.id = MemoryDomainId::random(); dom.kind = MemoryDomainKind::Accelerator;
    dom.device = DeviceId::random(); dom.backend = BackendType::Cuda; dom.capability = ComputeCapability::Sm_120;
    dom.total_capacity = Bytes::from_mib(1024); dom.governed_capacity = Bytes::from_mib(1024);
    coord.register_domain(std::move(dom));
    AdmitRequest req; req.model = model;
    LoadOutcome o = coord.load(req, AuthorityFrame{});
    std::cout << "loaded: " << (o.success ? "yes" : "no") << std::endl;
    coord.save_to(path);
    std::cout << "saved authoritative metadata" << std::endl;
  }
  {
    MemoryBackend backend;
    Coordinator coord(&backend, ResidencyPolicy{});
    coord.load_from(path);
    const CoordinatorSnapshot snap = coord.snapshot();
    std::cout << "recovered " << snap.residencies.size() << " residencies; "
              << "handle-free, readiness revalidation required" << std::endl;
  }
  std::remove(path.c_str());
  return 0;
}
