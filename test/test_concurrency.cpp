#include "mr_test.hpp"

#include <atomic>
#include <thread>
#include <vector>

#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"

namespace {

mr::ModelRevision make_model(mr::ModelId model, mr::ModelRevisionId rev, std::uint64_t bytes) {
  mr::ModelRevision m;
  m.model = model; m.revision = rev; m.version = "1.0";
  m.artifact.id = mr::ArtifactId::random(); m.artifact.generation = mr::ArtifactGeneration::first();
  m.artifact.backend = mr::BackendType::Cuda; m.artifact.capability = mr::ComputeCapability::Sm_120;
  m.backend_compat = {mr::BackendType::Cuda}; m.device_compat = {mr::ComputeCapability::Sm_120};
  m.required_memory = mr::Bytes(bytes); m.required_device_memory = mr::Bytes(bytes);
  return m;
}

} // namespace

MR_TEST(concurrency_high_contention) {
  mr::MemoryBackend backend;
  mr::Coordinator coord(&backend, mr::ResidencyPolicy{});
  const mr::DeviceId dev = mr::DeviceId::random();
  mr::MemoryDomain d;
  d.id = mr::MemoryDomainId::random(); d.kind = mr::MemoryDomainKind::Accelerator; d.device = dev;
  d.backend = mr::BackendType::Cuda; d.capability = mr::ComputeCapability::Sm_120;
  d.total_capacity = mr::Bytes::from_mib(1024); d.governed_capacity = mr::Bytes::from_mib(1024);
  coord.register_domain(d);

  constexpr int kModels = 4;
  std::vector<mr::ModelId> models;
  for (int i = 0; i < kModels; ++i) {
    const mr::ModelId m = mr::ModelId::random();
    coord.define_model(make_model(m, mr::ModelRevisionId::random(), 4096));
    models.push_back(m);
  }

  constexpr int kThreads = 8;
  constexpr int kIters = 150;
  std::vector<std::thread> threads;
  std::atomic<int> failures{0};
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&, t] {
      for (int i = 0; i < kIters; ++i) {
        const mr::ModelId m = models[(t + i) % kModels];
        mr::AdmitRequest req; req.model = m;
        mr::LoadOutcome o;
        try { o = coord.load(req, mr::AuthorityFrame{}); } catch (const mr::Error&) { ++failures; continue; }
        if (o.success && (i % 4) == 0) {
          mr::EvictRequest ev; ev.residency = o.residency; ev.reason = mr::EvictionReason::Manual;
          try { coord.evict(ev, mr::AuthorityFrame{}); } catch (const mr::Error&) {}
        }
      }
    });
  }
  for (auto& th : threads) { th.join(); }
  MR_ASSERT(failures.load() == 0);

  // Accounting invariant after all threads joined.
  for (const auto& did : coord.domains().ids()) {
    const mr::MemoryDomain* dd = coord.domains().find(did);
    if (dd) { dd->verify_invariants(); }
  }
}
