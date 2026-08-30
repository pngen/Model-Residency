#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "mr/backends/cuda_backend.hpp"
#include "mr/coordinator.hpp"

namespace {

mr::ModelRevision make_model(mr::ModelId model, mr::ModelRevisionId rev, std::uint64_t bytes) {
  mr::ModelRevision m;
  m.model = model; m.revision = rev; m.version = "1.0";
  m.artifact.id = mr::ArtifactId::random(); m.artifact.generation = mr::ArtifactGeneration::first();
  m.artifact.backend = mr::BackendType::Cuda; m.artifact.capability = mr::ComputeCapability::Sm_120;
  m.artifact.dtype = mr::DataType::Fp16;
  m.backend_compat = {mr::BackendType::Cuda}; m.device_compat = {mr::ComputeCapability::Sm_120};
  m.required_memory = mr::Bytes(bytes); m.required_device_memory = mr::Bytes(bytes);
  return m;
}

int fail(const char* msg) {
  std::fprintf(stderr, "CUDA PROOF FAILED: %s\n", msg);
  return 1;
}

} // namespace

int main() {
  using namespace mr;

  CudaBackend backend;
  if (!backend.usable()) { return fail("no usable CUDA device"); }

  const std::size_t baseline = CudaBackend::free_bytes();
  std::cout << "CUDA proof: baseline free = " << baseline << " bytes\n";

  Coordinator coord(&backend, ResidencyPolicy{});
  const ModelId model = ModelId::random();
  const ModelRevisionId rev = ModelRevisionId::random();
  const std::uint64_t weight_bytes = 8ull * 1024ull * 1024ull;
  coord.define_model(make_model(model, rev, weight_bytes));
  MemoryDomain dom;
  dom.id = MemoryDomainId::random(); dom.kind = MemoryDomainKind::Accelerator;
  dom.device = DeviceId::random(); dom.backend = BackendType::Cuda; dom.capability = ComputeCapability::Sm_120;
  dom.total_capacity = Bytes::from_mib(2048); dom.governed_capacity = Bytes::from_mib(2048);
  coord.register_domain(dom);

  AdmitRequest req; req.model = model; req.model_revision = rev;

  LoadOutcome o = coord.load(req, AuthorityFrame{});
  if (!o.success) { return fail("cold load failed"); }
  const Residency* r = coord.residency(o.residency);
  if (r == nullptr || r->handle == nullptr) { return fail("cold load: no resident handle"); }
  if (r->lifecycle != LifecycleState::Ready) { return fail("cold load: not READY"); }
  std::cout << "STEP 1 cold load: residency READY (device)\n";

  std::vector<float> ref = CudaBackend::make_host_weights(*coord.models().latest_revision(model));
  double result1 = 0.0; std::string err;
  if (!CudaBackend::run_and_verify(r->handle, ref, result1, err)) { return fail(("kernel verify 1: " + err).c_str()); }
  std::cout << "STEP 2 kernel over resident state OK (result=" << result1 << ")\n";

  LoadOutcome o2 = coord.load(req, AuthorityFrame{});
  if (!o2.success || o2.residency != o.residency) { return fail("reuse did not reuse residency"); }
  double result2 = 0.0;
  if (!CudaBackend::run_and_verify(r->handle, ref, result2, err)) { return fail(("kernel verify 2: " + err).c_str()); }
  if (result1 != result2) { return fail("reuse changed resident state"); }
  std::cout << "STEP 3 warm reuse: same residency, re-executed without reloading\n";

  EvictRequest ev; ev.residency = o.residency; ev.reason = EvictionReason::Manual;
  EvictionOutcome eo = coord.evict(ev, AuthorityFrame{});
  if (!eo.success) { return fail("evict failed"); }
  const std::size_t after_evict = CudaBackend::free_bytes();
  if (after_evict < baseline - (1ull << 20)) { return fail("device memory not recovered after evict"); }
  std::cout << "STEP 4 evict: device memory recovered (free=" << after_evict << ")\n";

  LoadOutcome o3 = coord.load(req, AuthorityFrame{});
  if (!o3.success) { return fail("reload failed"); }
  const Residency* r3 = coord.residency(o3.residency);
  if (r3 == nullptr || r3->handle == nullptr || r3->generation == r->generation) { return fail("reload: no fresh residency"); }
  double result3 = 0.0;
  if (!CudaBackend::run_and_verify(r3->handle, ref, result3, err)) { return fail(("kernel verify 3: " + err).c_str()); }
  std::cout << "STEP 5 reload under new ResidencyGeneration + re-execute OK\n";

  EvictRequest ev2; ev2.residency = o3.residency; ev2.reason = EvictionReason::Manual;
  if (!coord.evict(ev2, AuthorityFrame{}).success) { return fail("final evict failed"); }
  const std::size_t final_free = CudaBackend::free_bytes();
  if (final_free < baseline - (1ull << 20)) { return fail("final device memory not recovered"); }
  std::cout << "STEP 6 teardown: final free=" << final_free << " (baseline=" << baseline << ") -> recovered\n";
  std::cout << "CUDA RESIDENCY PROOF PASSED\n";
  return 0;
}
