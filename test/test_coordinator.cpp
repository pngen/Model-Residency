#include "mr_test.hpp"

#include "mr/backend.hpp"
#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"
#include "mr/policy.hpp"

namespace {

mr::ModelRevision make_model(mr::ModelId model, mr::ModelRevisionId rev, std::uint64_t bytes) {
  mr::ModelRevision m;
  m.model = model;
  m.revision = rev;
  m.version = "1.0";
  m.artifact.id = mr::ArtifactId::random();
  m.artifact.generation = mr::ArtifactGeneration::first();
  m.artifact.backend = mr::BackendType::Cuda;
  m.artifact.capability = mr::ComputeCapability::Sm_120;
  m.artifact.dtype = mr::DataType::Fp16;
  m.backend_compat = {mr::BackendType::Cuda};
  m.device_compat = {mr::ComputeCapability::Sm_120};
  m.required_memory = mr::Bytes(bytes);
  m.required_device_memory = mr::Bytes(bytes);
  return m;
}

mr::MemoryDomain make_domain(mr::DeviceId dev, std::uint64_t bytes) {
  mr::MemoryDomain d;
  d.id = mr::MemoryDomainId::random();
  d.kind = mr::MemoryDomainKind::Accelerator;
  d.device = dev;
  d.backend = mr::BackendType::Cuda;
  d.capability = mr::ComputeCapability::Sm_120;
  d.total_capacity = mr::Bytes(bytes);
  d.governed_capacity = mr::Bytes(bytes);
  return d;
}

} // namespace

MR_TEST(coord_load_and_reuse) {
  mr::MemoryBackend backend;
  mr::ResidencyPolicy policy;
  mr::Coordinator coord(&backend, policy);

  const mr::ModelId model = mr::ModelId::random();
  const mr::ModelRevisionId rev = mr::ModelRevisionId::random();
  coord.define_model(make_model(model, rev, 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));

  mr::AdmitRequest req;
  req.model = model;
  req.model_revision = rev;

  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(o.success);
  MR_ASSERT(o.loaded.value() == 512);

  const mr::Residency* r = coord.residency(o.residency);
  MR_ASSERT(r != nullptr);
  MR_ASSERT(r->lifecycle == mr::LifecycleState::Ready);
  MR_ASSERT(r->authority == mr::AuthorityState::Authoritative);
  MR_ASSERT(r->readiness == mr::ReadinessState::Ready);

  mr::LoadOutcome o2 = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(o2.success);
  MR_ASSERT(o2.residency == o.residency);

  MR_ASSERT(backend.live_buffers() == 1);
}

MR_TEST(coord_capacity_rejection) {
  mr::MemoryBackend backend;
  mr::ResidencyPolicy policy;
  mr::Coordinator coord(&backend, policy);
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 8192));
  coord.register_domain(make_domain(mr::DeviceId::random(), 128));

  mr::AdmitRequest req;
  req.model = model;
  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(!o.success);
}

MR_TEST(coord_pin_protects_eviction) {
  mr::MemoryBackend backend;
  mr::ResidencyPolicy policy;
  mr::Coordinator coord(&backend, policy);
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));

  mr::AdmitRequest req;
  req.model = model;
  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(o.success);
  coord.pin(o.residency);

  mr::EvictRequest ev;
  ev.residency = o.residency;
  ev.reason = mr::EvictionReason::CapacityPressure;
  mr::EvictionOutcome eo = coord.evict(ev, mr::AuthorityFrame{});
  MR_ASSERT(!eo.success); // pinned, not forced
  MR_ASSERT(backend.live_buffers() == 1);

  coord.unpin(o.residency);
  eo = coord.evict(ev, mr::AuthorityFrame{});
  MR_ASSERT(eo.success);
  MR_ASSERT(backend.live_buffers() == 0);
}

MR_TEST(coord_generation_rollover) {
  mr::MemoryBackend backend;
  mr::ResidencyPolicy policy;
  mr::Coordinator coord(&backend, policy);
  const mr::ModelId model = mr::ModelId::random();
  const mr::ModelRevisionId r1 = mr::ModelRevisionId::random();
  const mr::ModelRevisionId r2 = mr::ModelRevisionId::random();
  coord.define_model(make_model(model, r1, 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));

  mr::AdmitRequest req;
  req.model = model;
  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(o.success);

  mr::ModelRevision nv = make_model(model, r2, 900);
  nv.artifact.generation = mr::ArtifactGeneration(2);
  coord.roll_model_generation(model, nv, mr::AuthorityFrame{});

  const mr::Residency* old = coord.residency(o.residency);
  MR_ASSERT(old != nullptr);
  MR_ASSERT(old->lifecycle == mr::LifecycleState::Invalidated);
  MR_ASSERT(old->authority == mr::AuthorityState::Stale);
  MR_ASSERT(backend.live_buffers() == 0);
}

MR_TEST(coord_last_authoritative_protected) {
  mr::MemoryBackend backend;
  mr::ResidencyPolicy policy;
  policy.allow_evict_last_authoritative = false;
  mr::Coordinator coord(&backend, policy);
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));

  mr::AdmitRequest req;
  req.model = model;
  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(o.success);

  mr::EvictRequest ev;
  ev.residency = o.residency;
  ev.reason = mr::EvictionReason::CapacityPressure;
  mr::EvictionOutcome eo = coord.evict(ev, mr::AuthorityFrame{});
  MR_ASSERT(!eo.success);
}
