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
  m.required_memory = Bytes(4096); m.required_device_memory = Bytes(4096);
  coord.define_model(std::move(m));

  AdapterSet aset;
  aset.id = AdapterSetId::random(); aset.base_model = model; aset.base_revision = m.revision;
  aset.generation = AdapterGeneration::first();
  Adapter ad;
  ad.id = AdapterId::random(); ad.base_model = model; ad.base_revision = m.revision;
  ad.revision = "adapter-v1"; ad.generation = AdapterGeneration::first(); ad.size = Bytes(1024);
  aset.adapters.push_back(ad); aset.aggregate_size = Bytes(1024);
  coord.register_adapter_set(std::move(aset));

  MemoryDomain dom;
  dom.id = MemoryDomainId::random(); dom.kind = MemoryDomainKind::Accelerator;
  dom.device = DeviceId::random(); dom.backend = BackendType::Cuda; dom.capability = ComputeCapability::Sm_120;
  dom.total_capacity = Bytes::from_mib(1024); dom.governed_capacity = Bytes::from_mib(1024);
  coord.register_domain(std::move(dom));

  AdmitRequest req; req.model = model; req.adapter_set = aset.id;
  LoadOutcome o = coord.load(req, AuthorityFrame{});
  std::cout << "model+adapter load " << (o.success ? "succeeded" : "failed: " + o.error) << std::endl;
  const Residency* r = coord.residency(o.residency);
  if (r) { std::cout << "adapter_set=" << r->adapter_set.to_hex() << " ready=" << r->is_execution_ready() << std::endl; }
  return o.success ? 0 : 1;
}
