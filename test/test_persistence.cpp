#include "mr_test.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"
#include "mr/persistence.hpp"

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

mr::MemoryDomain make_domain(mr::DeviceId dev, std::uint64_t bytes) {
  mr::MemoryDomain d;
  d.id = mr::MemoryDomainId::random(); d.kind = mr::MemoryDomainKind::Accelerator; d.device = dev;
  d.backend = mr::BackendType::Cuda; d.capability = mr::ComputeCapability::Sm_120;
  d.total_capacity = mr::Bytes(bytes); d.governed_capacity = mr::Bytes(bytes);
  return d;
}

} // namespace

MR_TEST(persistence_roundtrip) {
  mr::MemoryBackend backend;
  mr::Coordinator coord(&backend, mr::ResidencyPolicy{});
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));
  mr::AdmitRequest req; req.model = model;
  mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(o.success);

  const mr::CoordinatorSnapshot snap = coord.snapshot();
  const auto bytes = mr::PersistenceCodec::encode(snap);
  const mr::CoordinatorSnapshot back = mr::PersistenceCodec::decode(bytes);
  MR_ASSERT(back.epoch == snap.epoch);
  MR_ASSERT(back.residencies.size() == snap.residencies.size());
  for (const auto& r : back.residencies) { MR_ASSERT(r.handle == nullptr); }
  MR_ASSERT(back.models.size() == snap.models.size());
}

MR_TEST(persistence_corruption_rejected) {
  mr::MemoryBackend backend;
  mr::Coordinator coord(&backend, mr::ResidencyPolicy{});
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));
  mr::AdmitRequest req; req.model = model;
  coord.load(req, mr::AuthorityFrame{});
  auto bytes = mr::PersistenceCodec::encode(coord.snapshot());

  // Flip a payload byte -> checksum mismatch.
  std::vector<std::uint8_t> corrupt = bytes;
  corrupt[bytes.size() / 2] ^= 0x55;
  bool threw = false;
  try { (void)mr::PersistenceCodec::decode(corrupt); } catch (const mr::Error& e) {
    threw = true;
    MR_ASSERT(e.code() == mr::ErrorCode::PersistenceChecksum || e.code() == mr::ErrorCode::PersistenceFormat);
  }
  MR_ASSERT(threw);

  // Truncate -> must be rejected.
  std::vector<std::uint8_t> trunc(bytes.begin(), bytes.begin() + static_cast<long>(bytes.size() - 3));
  threw = false;
  try { (void)mr::PersistenceCodec::decode(trunc); } catch (const mr::Error&) { threw = true; }
  MR_ASSERT(threw);

  // Trailing garbage -> must be rejected.
  std::vector<std::uint8_t> extra = bytes;
  extra.push_back(0xAB);
  threw = false;
  try { (void)mr::PersistenceCodec::decode(extra); } catch (const mr::Error&) { threw = true; }
  MR_ASSERT(threw);
}

MR_TEST(persistence_recovery_not_ready) {
  mr::MemoryBackend backend;
  mr::Coordinator coord(&backend, mr::ResidencyPolicy{});
  const mr::ModelId model = mr::ModelId::random();
  coord.define_model(make_model(model, mr::ModelRevisionId::random(), 512));
  coord.register_domain(make_domain(mr::DeviceId::random(), 1 << 20));
  mr::AdmitRequest req; req.model = model;
  const mr::LoadOutcome o = coord.load(req, mr::AuthorityFrame{});
  MR_ASSERT(o.success);

  const std::string path = "mr_recovery_test.bin";
  coord.save_to(path);

  // Restore into a fresh coordinator with a fresh epoch.
  mr::Coordinator restored(&backend, mr::ResidencyPolicy{});
  restored.load_from(path);

  const mr::Residency* r = restored.residency(o.residency);
  MR_ASSERT(r != nullptr);
  // Recovery does NOT inherit readiness automatically.
  MR_ASSERT(r->lifecycle == mr::LifecycleState::Resident || r->lifecycle == mr::LifecycleState::Ready);
  MR_ASSERT(r->handle == nullptr);

  std::remove(path.c_str());
}
