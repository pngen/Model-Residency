#include "mr_test.hpp"

#include <random>

#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"
#include "mr/policy.hpp"

namespace {

mr::ModelRevision make_model(mr::ModelId model, mr::ModelRevisionId rev, std::uint64_t bytes) {
  mr::ModelRevision m;
  m.model = model; m.revision = rev; m.version = "1.";
  m.artifact.id = mr::ArtifactId::random(); m.artifact.generation = mr::ArtifactGeneration::first();
  m.artifact.backend = mr::BackendType::Cuda; m.artifact.capability = mr::ComputeCapability::Sm_120;
  m.backend_compat = {mr::BackendType::Cuda}; m.device_compat = {mr::ComputeCapability::Sm_120};
  m.required_memory = mr::Bytes(bytes); m.required_device_memory = mr::Bytes(bytes);
  return m;
}

mr::MemoryDomain make_domain_for(mr::DeviceId dev, std::uint64_t bytes,
                             mr::MemoryDomainKind kind, mr::ResidencyClass cls) {
  mr::MemoryDomain d;
  d.id = mr::MemoryDomainId::random(); d.kind = kind; d.device = dev;
  d.backend = mr::BackendType::Cuda; d.capability = mr::ComputeCapability::Sm_120;
  d.total_capacity = mr::Bytes(bytes); d.governed_capacity = mr::Bytes(bytes);
  (void)cls;
  return d;
}

// Continuous invariants that must hold after every settled operation.
void assert_invariants(const mr::Coordinator& coord) {
  // Per-domain accounting is exact (no overcommit / negative).
  for (const auto& did : coord.domains().ids()) {
    const mr::MemoryDomain* d = coord.domains().find(did);
    if (d) { d->verify_invariants(); }
  }
  // No duplicate READY authoritative residency per (model, model_generation).
  for (const auto& [rid, r] : coord.residencies()) {
    if (r.lifecycle != mr::LifecycleState::Ready || r.authority != mr::AuthorityState::Authoritative) continue;
    for (const auto& [rid2, r2] : coord.residencies()) {
      if (rid2 == rid) continue;
      if (r2.model == r.model && r2.model_generation == r.model_generation &&
          r2.lifecycle == mr::LifecycleState::Ready && r2.authority == mr::AuthorityState::Authoritative) {
        MR_ASSERT_MSG(false, "duplicate READY authoritative residency published");
      }
    }
  }
}

} // namespace

MR_TEST(property_residency_churn) {
  mr::MemoryBackend backend;
  mr::Coordinator coord(&backend, mr::ResidencyPolicy{});
  const mr::ModelId model = mr::ModelId::random();
  const std::uint64_t model_bytes = 512;
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), model_bytes));
  const mr::DeviceId dev = mr::DeviceId::random();
  coord.register_domain(make_domain_for(dev, 1 << 20, mr::MemoryDomainKind::Accelerator, mr::ResidencyClass::AcceleratorLocal));
  coord.register_domain(make_domain_for(mr::DeviceId::nil(), 1 << 20, mr::MemoryDomainKind::PinnedHost, mr::ResidencyClass::PinnedHost));

  std::mt19937_64 rng(0x9e3779b97f4a7c15ULL);
  for (int step = 0; step < 400; ++step) {
    const int op = static_cast<int>(rng() % 5);
    mr::AdmitRequest req; req.model = model;
    switch (op) {
      case 0: {
        mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
        (void)o;
        break;
      }
      case 1: {
        // Pin/unpin a ready residency.
        for (const auto& [rid, r] : coord.residencies()) {
          if (r.lifecycle == mr::LifecycleState::Ready) {
            if ((rng() & 1) == 0) { try { coord.pin(rid); } catch (const mr::Error&) {} }
            else { try { coord.unpin(rid); } catch (const mr::Error&) {} }
          }
        }
        break;
      }
      case 2: {
        // Try to evict a residency.
        mr::EvictRequest ev;
        ev.reason = mr::EvictionReason::CapacityPressure;
        for (const auto& [rid, r] : coord.residencies()) {
          if (r.lifecycle == mr::LifecycleState::Ready && !r.pinned) {
            ev.residency = rid;
            coord.evict(ev, mr::AuthorityFrame{});
            break;
          }
        }
        break;
      }
      case 3: {
        // Migrate a ready residency to pinned host if there is a host domain.
        mr::MigrateRequest mr_;
        for (const auto& [rid, r] : coord.residencies()) {
          if (r.lifecycle == mr::LifecycleState::Ready) {
            mr_.residency = rid; mr_.dest_class = mr::ResidencyClass::PinnedHost;
            coord.migrate(mr_, mr::AuthorityFrame{});
            break;
          }
        }
        break;
      }
      case 4: {
        coord.rebalance(mr::AuthorityFrame{});
        break;
      }
    }
    assert_invariants(coord);
  }

  // Tear down: evict everything and verify accounting returns to baseline.
  std::vector<mr::ResidencyId> ids;
  for (const auto& [rid, r] : coord.residencies()) { if (r.is_resident_state()) ids.push_back(rid); }
  for (const auto& id : ids) {
    mr::EvictRequest ev; ev.residency = id; ev.reason = mr::EvictionReason::Manual;
    coord.evict(ev, mr::AuthorityFrame{});
  }
  for (const auto& did : coord.domains().ids()) {
    const mr::MemoryDomain* d = coord.domains().find(did);
    if (d) { MR_ASSERT(d->resident().is_zero()); MR_ASSERT(d->reserved().is_zero()); }
  }
  MR_ASSERT(backend.live_buffers() == 0);
}
