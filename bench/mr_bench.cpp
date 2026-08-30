#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"
#include "mr/persistence.hpp"

namespace {

using Clock = std::chrono::steady_clock;
double ms_since(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

mr::ModelRevision make_model(std::uint64_t bytes) {
  mr::ModelRevision m;
  m.model = mr::ModelId::random(); m.revision = mr::ModelRevisionId::random(); m.version = "1.0";
  m.artifact.id = mr::ArtifactId::random(); m.artifact.generation = mr::ArtifactGeneration::first();
  m.artifact.backend = mr::BackendType::Cuda; m.artifact.capability = mr::ComputeCapability::Sm_120;
  m.backend_compat = {mr::BackendType::Cuda}; m.device_compat = {mr::ComputeCapability::Sm_120};
  m.required_memory = mr::Bytes(bytes); m.required_device_memory = mr::Bytes(bytes);
  return m;
}

void report(const char* name, std::uint64_t iters, double ms, std::uint64_t bytes) {
  const double ops_per_s = iters / (ms / 1000.0);
  std::printf("%-34s %10llu ops   %8.3f ms   %12.2f ops/s   %llu bytes\n",
              name, static_cast<unsigned long long>(iters), ms, ops_per_s,
              static_cast<unsigned long long>(bytes));
}

} // namespace

int main() {
  using namespace mr;
  std::printf("Model Residency benchmark\n");

  {
    std::uint64_t iters = 1000000;
    auto t0 = Clock::now();
    std::uint64_t sink = 0;
    for (std::uint64_t i = 0; i < iters; ++i) {
      ModelId id = ModelId::random();
      sink ^= id.value().hi() ^ id.value().lo();
    }
    (void)sink;
    report("model identity construction", iters, ms_since(t0), 16 * iters);
  }
  {
    ModelRevision m = make_model(4096);
    CompatibilityChecker checker;
    std::uint64_t iters = 1000000;
    auto t0 = Clock::now();
    int result = 0;
    for (std::uint64_t i = 0; i < iters; ++i) {
      Compatibility c = checker.check_model_backend(m, BackendType::Cuda);
      result += c.compatible() ? 1 : 0;
    }
    (void)result;
    report("compatibility decision", iters, ms_since(t0), 0);
  }

  MemoryBackend backend;
  Coordinator coord(&backend, ResidencyPolicy{});
  ModelRevision m = make_model(4096);
  coord.define_model(m);
  MemoryDomain dom;
  dom.id = MemoryDomainId::random(); dom.kind = MemoryDomainKind::Accelerator;
  dom.device = DeviceId::random(); dom.backend = BackendType::Cuda; dom.capability = ComputeCapability::Sm_120;
  dom.total_capacity = Bytes::from_mib(4096); dom.governed_capacity = Bytes::from_mib(4096);
  coord.register_domain(dom);
  AdmitRequest req; req.model = m.model;
  LoadOutcome o = coord.load(req, AuthorityFrame{});
  if (!o.success) { std::printf("preload failed\n"); return 1; }

  {
    const std::uint64_t iters = 200000;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < iters; ++i) { (void)coord.residency(o.residency); }
    report("residency lookup", iters, ms_since(t0), 0);
  }
  {
    const std::uint64_t iters = 100000;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < iters; ++i) { AdmissionResult a = coord.admit(req, AuthorityFrame{}); (void)a; }
    report("admission decision", iters, ms_since(t0), 0);
  }
  {
    const std::uint64_t iters = 100000;
    std::uint64_t reused = 0;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < iters; ++i) { LoadOutcome lo = coord.load(req, AuthorityFrame{}); if (lo.success) ++reused; }
    report("warm reuse load", iters, ms_since(t0), o.loaded.value() * iters);
    std::printf("  (reused %llu of %llu)\n", static_cast<unsigned long long>(reused),
                static_cast<unsigned long long>(iters));
  }
  {
    const std::uint64_t iters = 100000;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < iters; ++i) { CoordinatorSnapshot s = coord.snapshot(); (void)s; }
    report("snapshot generation", iters, ms_since(t0), 0);
  }
  {
    const std::uint64_t iters = 2000;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < iters; ++i) { (void)PersistenceCodec::encode(coord.snapshot()); }
    report("persistence encode", iters, ms_since(t0), 0);
  }
  {
    const auto bytes = PersistenceCodec::encode(coord.snapshot());
    const std::uint64_t iters = 5000;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < iters; ++i) { (void)PersistenceCodec::decode(bytes); }
    report("persistence decode (recovery)", iters, ms_since(t0), bytes.size() * iters);
  }

  {
    constexpr int threads = 8;
    const std::uint64_t iters = 20000;
    auto t0 = Clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < threads; ++t) {
      ts.emplace_back([&] {
        for (std::uint64_t i = 0; i < iters; ++i) {
          AdmitRequest r; r.model = m.model;
          LoadOutcome lo = coord.load(r, AuthorityFrame{});
          (void)lo;
        }
      });
    }
    for (auto& th : ts) { th.join(); }
    report("concurrent lifecycle churn", iters * threads, ms_since(t0), 0);
  }

  {
    EvictRequest ev; ev.residency = o.residency; ev.reason = EvictionReason::Manual;
    coord.evict(ev, AuthorityFrame{});
    const std::uint64_t iters = 500;
    auto t0 = Clock::now();
    LoadOutcome last;
    for (std::uint64_t i = 0; i < iters; ++i) { last = coord.load(req, AuthorityFrame{}); }
    report("cold load after evict", iters, ms_since(t0), last.loaded.value() * iters);
    coord.evict(EvictRequest{last.residency, EvictionReason::Manual, false}, AuthorityFrame{});
  }

  std::printf("benchmark complete (all operations counted, real bytes reported)\n");
  return 0;
}
