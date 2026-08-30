#include "mr_test.hpp"

#include <atomic>

#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"
#include "mr/core/serialization.hpp"

namespace {

// A backend that injects a failure into the Nth load call, to exercise
// failed-load rollback and partial residency.
class FailBackend : public mr::MemoryBackend {
 public:
  mr::LoadResult load(const mr::LoadTarget& t, const mr::ModelRevision& m, const mr::AdapterSet* a) override {
    const int n = counter_.fetch_add(1);
    if (fail_index_ >= 0 && n == fail_index_) {
      mr::LoadResult r;
      r.success = false;
      r.error = "injected failure";
      return r;
    }
    return mr::MemoryBackend::load(t, m, a);
  }
  int fail_index_ = -1;
  std::atomic<int> counter_{0};
};

mr::ModelRevision make_model(mr::ModelId model, mr::ModelRevisionId rev, std::uint64_t bytes,
                             std::uint32_t shards = 0) {
  mr::ModelRevision m;
  m.model = model; m.revision = rev; m.version = "1.0";
  m.artifact.id = mr::ArtifactId::random(); m.artifact.generation = mr::ArtifactGeneration::first();
  m.artifact.backend = mr::BackendType::Cuda; m.artifact.capability = mr::ComputeCapability::Sm_120;
  m.backend_compat = {mr::BackendType::Cuda}; m.device_compat = {mr::ComputeCapability::Sm_120};
  if (shards == 0) {
    m.required_memory = mr::Bytes(bytes); m.required_device_memory = mr::Bytes(bytes);
  } else {
    for (std::uint32_t i = 0; i < shards; ++i) {
      mr::ShardDescriptor s;
      s.id = mr::ShardId::random(); s.index = i; s.total = shards; s.size = mr::Bytes(bytes / shards);
      s.generation = mr::ArtifactGeneration::first(); s.required = true;
      m.shards.push_back(s);
    }
    m.required_memory = mr::Bytes(bytes);
  }
  return m;
}

mr::MemoryDomain make_domain(mr::DeviceId dev, std::uint64_t bytes) {
  mr::MemoryDomain d;
  d.id = mr::MemoryDomainId::random(); d.kind = mr::MemoryDomainKind::Accelerator; d.device = dev;
  d.backend = mr::BackendType::Cuda; d.capability = mr::ComputeCapability::Sm_120;
  d.total_capacity = mr::Bytes(bytes); d.governed_capacity = mr::Bytes(bytes);
  return d;
}

} // namespace

MR_TEST(adversarial_failed_load_rollback) {
  FailBackend backend;
  backend.fail_index_ = 0;
  mr::Coordinator coord(&backend, mr::ResidencyPolicy{});
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));

  mr::AdmitRequest req; req.model = model;
  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(!o.success);
  // Reservation is cancelled; accounting is exact.
  for (const auto& did : coord.domains().ids()) {
    const mr::MemoryDomain* d = coord.domains().find(did);
    if (d) { MR_ASSERT(d->resident().is_zero()); MR_ASSERT(d->reserved().is_zero()); }
  }
}

MR_TEST(adversarial_partial_shard_not_ready) {
  FailBackend backend;
  backend.fail_index_ = 1; // the second shard fails
  mr::Coordinator coord(&backend, mr::ResidencyPolicy{});
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 1024, 2));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));

  mr::AdmitRequest req; req.model = model;
  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(!o.success);
  // The set did not become ready (partial residency is explicit).
  for (const auto& [sid, set] : coord.sets()) {
    MR_ASSERT(!set.ready);
  }
}

MR_TEST(adversarial_active_reference_blocks_eviction) {
  mr::MemoryBackend backend;
  mr::Coordinator coord(&backend, mr::ResidencyPolicy{});
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));
  mr::AdmitRequest req; req.model = model;
  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(o.success);

  mr::Residency& r = *const_cast<mr::Residency*>(coord.residency(o.residency));
  r.active_references = 1;
  mr::EvictRequest ev; ev.residency = o.residency; ev.reason = mr::EvictionReason::CapacityPressure;
  mr::EvictionOutcome eo = coord.evict(ev, mr::AuthorityFrame{});
  MR_ASSERT(!eo.success);
}

MR_TEST(adversarial_stale_authority_rejected) {
  mr::MemoryBackend backend;
  mr::Coordinator coord(&backend, mr::ResidencyPolicy{});
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));
  mr::AdmitRequest req; req.model = model;
  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(o.success);

  // A stale coordinator epoch must be rejected at every authority gate.
  mr::AuthorityFrame stale;
  stale.coordinator_epoch = mr::CoordinatorEpoch(99); // differs from current and is nonzero
  bool threw = false;
  try { coord.publish_ready(o.residency, stale); } catch (const mr::Error&) { threw = true; }
  MR_ASSERT(threw);

  threw = false;
  try { coord.evict(mr::EvictRequest{/*residency*/o.residency, mr::EvictionReason::Manual, false}, stale); }
  catch (const mr::Error&) { threw = true; }
  MR_ASSERT(threw);
}

MR_TEST(adversarial_binary_reader_bounds) {
  // A malformed/truncated buffer must be rejected by the bounded reader.
  std::vector<std::uint8_t> junk = {1, 2, 3, 4};
  mr::BinReader r(junk.data(), junk.size());
  bool threw = false;
  try { (void)r.u64(); } catch (const mr::Error& e) { threw = true; MR_ASSERT(e.code() == mr::ErrorCode::ProtocolTruncation); }
  MR_ASSERT(threw);
}

MR_TEST(adversarial_double_release_rejected) {
  mr::MemoryDomain d;
  d.total_capacity = mr::Bytes(100); d.governed_capacity = mr::Bytes(100);
  d.reserve(mr::Bytes(50)); d.commit(mr::Bytes(50));
  d.release(mr::Bytes(50));
  bool threw = false;
  try { d.release(mr::Bytes(10)); } catch (const mr::Error&) { threw = true; }
  MR_ASSERT(threw);
}
