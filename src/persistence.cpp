#include "mr/persistence.hpp"

#include <set>
#include <string>

#include "mr/core/checksum.hpp"
#include "mr/core/error.hpp"
#include "mr/core/serialization.hpp"

namespace mr {
namespace {

std::uint32_t count_checked(BinReader& r) {
  const std::uint32_t n = r.u32();
  if (n > (1u << 20)) { throw Error(ErrorCode::PersistenceCorruption, "implausible count"); }
  return n;
}

void wgen(BinWriter& w, std::uint64_t v) { w.u64(v); }
std::uint64_t rgen(BinReader& r) { return r.u64(); }

void enc_shard(BinWriter& w, const ShardDescriptor& s) {
  w.uid(s.id.value()); w.u8(static_cast<std::uint8_t>(s.role)); w.u32(s.index); w.u32(s.total);
  w.bytes(s.size); w.string(s.content_digest); wgen(w, s.generation.value()); w.boolean(s.required);
  w.u32(static_cast<std::uint32_t>(s.backend_compat.size()));
  for (auto b : s.backend_compat) { w.u8(static_cast<std::uint8_t>(b)); }
}
ShardDescriptor dec_shard(BinReader& r) {
  ShardDescriptor s;
  s.id = ShardId::from_bytes(r.uid().bytes()); s.role = shard_role_from_int(r.u8());
  s.index = r.u32(); s.total = r.u32(); s.size = r.bytes(); s.content_digest = r.string();
  s.generation = ArtifactGeneration(rgen(r)); s.required = r.boolean();
  const std::uint32_t n = count_checked(r);
  for (std::uint32_t i = 0; i < n; ++i) { s.backend_compat.push_back(backend_type_from_int(r.u8())); }
  return s;
}
void enc_artifact(BinWriter& w, const ArtifactDescriptor& a) {
  w.uid(a.id.value()); wgen(w, a.generation.value());
  w.u8(static_cast<std::uint8_t>(a.format)); w.u8(static_cast<std::uint8_t>(a.dtype));
  w.u8(static_cast<std::uint8_t>(a.mode)); w.bytes(a.size); w.string(a.content_digest);
  w.uid(a.tokenizer_id.value()); w.string(a.architecture);
  w.u8(static_cast<std::uint8_t>(a.backend)); w.u8(static_cast<std::uint8_t>(a.capability));
  w.string(a.policy_fingerprint); w.boolean(a.engine.has_value());
  if (a.engine) { w.string(*a.engine); }
}
ArtifactDescriptor dec_artifact(BinReader& r) {
  ArtifactDescriptor a;
  a.id = ArtifactId::from_bytes(r.uid().bytes()); a.generation = ArtifactGeneration(rgen(r));
  a.format = model_format_from_int(r.u8()); a.dtype = data_type_from_int(r.u8());
  a.mode = numeric_mode_from_int(r.u8()); a.size = r.bytes(); a.content_digest = r.string();
  a.tokenizer_id = TokenizerId::from_bytes(r.uid().bytes()); a.architecture = r.string();
  a.backend = backend_type_from_int(r.u8()); a.capability = compute_capability_from_int(r.u8());
  a.policy_fingerprint = r.string();
  if (r.boolean()) { a.engine = r.string(); }
  return a;
}
void enc_model(BinWriter& w, const ModelRevision& m) {
  w.uid(m.model.value()); w.uid(m.revision.value()); w.string(m.version); enc_artifact(w, m.artifact);
  w.u32(static_cast<std::uint32_t>(m.shards.size()));
  for (const auto& s : m.shards) { enc_shard(w, s); }
  w.bytes(m.required_memory); w.bytes(m.required_device_memory);
  w.u32(static_cast<std::uint32_t>(m.backend_compat.size()));
  for (auto b : m.backend_compat) { w.u8(static_cast<std::uint8_t>(b)); }
  w.u32(static_cast<std::uint32_t>(m.device_compat.size()));
  for (auto c : m.device_compat) { w.u8(static_cast<std::uint8_t>(c)); }
  w.u32(m.max_parallel);
}
ModelRevision dec_model(BinReader& r) {
  ModelRevision m;
  m.model = ModelId::from_bytes(r.uid().bytes()); m.revision = ModelRevisionId::from_bytes(r.uid().bytes());
  m.version = r.string(); m.artifact = dec_artifact(r);
  const std::uint32_t ns = count_checked(r);
  for (std::uint32_t i = 0; i < ns; ++i) { m.shards.push_back(dec_shard(r)); }
  m.required_memory = r.bytes(); m.required_device_memory = r.bytes();
  const std::uint32_t nb = count_checked(r);
  for (std::uint32_t i = 0; i < nb; ++i) { m.backend_compat.push_back(backend_type_from_int(r.u8())); }
  const std::uint32_t nc = count_checked(r);
  for (std::uint32_t i = 0; i < nc; ++i) { m.device_compat.push_back(compute_capability_from_int(r.u8())); }
  m.max_parallel = r.u32();
  return m;
}
void enc_adapter(BinWriter& w, const Adapter& a) {
  w.uid(a.id.value()); w.uid(a.base_model.value()); w.uid(a.base_revision.value()); w.string(a.revision);
  wgen(w, a.generation.value()); w.u32(a.composition_order); w.bytes(a.size); w.string(a.content_digest);
  w.u8(static_cast<std::uint8_t>(a.format)); w.u8(static_cast<std::uint8_t>(a.dtype));
  w.boolean(a.active); w.string(a.policy_fingerprint);
}
Adapter dec_adapter(BinReader& r) {
  Adapter a;
  a.id = AdapterId::from_bytes(r.uid().bytes()); a.base_model = ModelId::from_bytes(r.uid().bytes());
  a.base_revision = ModelRevisionId::from_bytes(r.uid().bytes()); a.revision = r.string();
  a.generation = AdapterGeneration(rgen(r)); a.composition_order = r.u32(); a.size = r.bytes();
  a.content_digest = r.string(); a.format = model_format_from_int(r.u8()); a.dtype = data_type_from_int(r.u8());
  a.active = r.boolean(); a.policy_fingerprint = r.string();
  return a;
}
void enc_adapterset(BinWriter& w, const AdapterSet& s) {
  w.uid(s.id.value()); w.uid(s.base_model.value()); w.uid(s.base_revision.value()); wgen(w, s.generation.value());
  w.u32(static_cast<std::uint32_t>(s.adapters.size()));
  for (const auto& a : s.adapters) { enc_adapter(w, a); }
  w.bytes(s.aggregate_size); w.string(s.content_digest);
  w.u32(static_cast<std::uint32_t>(s.backend_compat.size()));
  for (auto b : s.backend_compat) { w.u8(static_cast<std::uint8_t>(b)); }
}
AdapterSet dec_adapterset(BinReader& r) {
  AdapterSet s;
  s.id = AdapterSetId::from_bytes(r.uid().bytes()); s.base_model = ModelId::from_bytes(r.uid().bytes());
  s.base_revision = ModelRevisionId::from_bytes(r.uid().bytes()); s.generation = AdapterGeneration(rgen(r));
  const std::uint32_t n = count_checked(r);
  for (std::uint32_t i = 0; i < n; ++i) { s.adapters.push_back(dec_adapter(r)); }
  s.aggregate_size = r.bytes(); s.content_digest = r.string();
  const std::uint32_t nb = count_checked(r);
  for (std::uint32_t i = 0; i < nb; ++i) { s.backend_compat.push_back(backend_type_from_int(r.u8())); }
  return s;
}
void enc_domain(BinWriter& w, const MemoryDomain& d) {
  w.uid(d.id.value()); w.u8(static_cast<std::uint8_t>(d.kind)); w.uid(d.device.value()); w.uid(d.node.value());
  w.u8(static_cast<std::uint8_t>(d.backend)); w.u8(static_cast<std::uint8_t>(d.capability));
  w.bytes(d.total_capacity); w.bytes(d.governed_capacity); w.bytes(d.unavailable_capacity);
  wgen(w, d.generation.value()); w.bytes(d.reserved()); w.bytes(d.resident()); w.bytes(d.pinned());
}
MemoryDomain dec_domain(BinReader& r) {
  MemoryDomain d;
  d.id = MemoryDomainId::from_bytes(r.uid().bytes()); d.kind = memory_domain_kind_from_int(r.u8());
  d.device = DeviceId::from_bytes(r.uid().bytes()); d.node = NodeId::from_bytes(r.uid().bytes());
  d.backend = backend_type_from_int(r.u8()); d.capability = compute_capability_from_int(r.u8());
  d.total_capacity = r.bytes(); d.governed_capacity = r.bytes(); d.unavailable_capacity = r.bytes();
  d.generation = CapacityGeneration(rgen(r));
  const Bytes resv = r.bytes(); const Bytes resident = r.bytes(); const Bytes pinned = r.bytes();
  if (!resv.is_zero()) { d.reserve(resv); }
  if (!resident.is_zero()) {
    if (!d.reserve(resident)) { throw Error(ErrorCode::PersistenceCorruption, "domain reservation overflow"); }
    d.commit(resident);
  }
  if (!pinned.is_zero()) { if (!d.pin(pinned)) { throw Error(ErrorCode::PersistenceCorruption, "domain pin overflow"); } }
  return d;
}
void enc_residency(BinWriter& w, const mr::Residency& r) {
  w.uid(r.id.value()); w.uid(r.model.value()); w.uid(r.model_revision.value()); w.uid(r.artifact.value());
  wgen(w, r.artifact_generation.value()); w.uid(r.adapter_set.value());
  w.uid(r.node.value()); w.uid(r.device.value()); w.uid(r.domain.value());
  w.u8(static_cast<std::uint8_t>(r.backend)); w.u8(static_cast<std::uint8_t>(r.capability));
  w.u8(static_cast<std::uint8_t>(r.cls)); w.u8(static_cast<std::uint8_t>(r.domain_kind));
  w.bytes(r.byte_size); w.bytes(r.allocated_bytes); w.bytes(r.loaded_bytes);
  w.u8(static_cast<std::uint8_t>(r.validation)); w.u8(static_cast<std::uint8_t>(r.readiness));
  w.u8(static_cast<std::uint8_t>(r.compatibility)); w.u8(static_cast<std::uint8_t>(r.lifecycle));
  w.u8(static_cast<std::uint8_t>(r.authority)); wgen(w, r.generation.value());
  w.string(r.source_provenance); w.string(r.artifact_path);
  w.u64(static_cast<std::uint64_t>(r.load_time_ns)); w.u64(static_cast<std::uint64_t>(r.last_use_ns));
  w.u64(r.usage_count); w.u64(r.active_references); w.boolean(r.pinned);
  wgen(w, r.model_generation.value()); wgen(w, r.adapter_generation.value());
  wgen(w, r.device_generation.value()); wgen(w, r.policy_generation.value());
  w.uid(r.attempt.value()); w.uid(r.migration.value()); w.uid(r.eviction.value());
  w.u8(static_cast<std::uint8_t>(r.eviction_reason)); w.string(r.failed_reason);
}
mr::Residency dec_residency(BinReader& r) {
  mr::Residency res;
  res.id = mr::ResidencyId::from_bytes(r.uid().bytes()); res.model = mr::ModelId::from_bytes(r.uid().bytes());
  res.model_revision = mr::ModelRevisionId::from_bytes(r.uid().bytes());
  res.artifact = mr::ArtifactId::from_bytes(r.uid().bytes());
  res.artifact_generation = mr::ArtifactGeneration(rgen(r));
  res.adapter_set = mr::AdapterSetId::from_bytes(r.uid().bytes());
  res.node = mr::NodeId::from_bytes(r.uid().bytes()); res.device = mr::DeviceId::from_bytes(r.uid().bytes());
  res.domain = mr::MemoryDomainId::from_bytes(r.uid().bytes());
  res.backend = backend_type_from_int(r.u8()); res.capability = compute_capability_from_int(r.u8());
  res.cls = residency_class_from_int(r.u8()); res.domain_kind = memory_domain_kind_from_int(r.u8());
  res.byte_size = r.bytes(); res.allocated_bytes = r.bytes(); res.loaded_bytes = r.bytes();
  res.validation = validation_state_from_int(r.u8()); res.readiness = readiness_state_from_int(r.u8());
  res.compatibility = compatibility_state_from_int(r.u8()); res.lifecycle = lifecycle_state_from_int(r.u8());
  res.authority = authority_state_from_int(r.u8()); res.generation = mr::ResidencyGeneration(rgen(r));
  res.source_provenance = r.string(); res.artifact_path = r.string();
  res.load_time_ns = static_cast<mr::MonotonicNs>(r.u64()); res.last_use_ns = static_cast<mr::MonotonicNs>(r.u64());
  res.usage_count = r.u64(); res.active_references = r.u64(); res.pinned = r.boolean();
  res.model_generation = mr::ModelGeneration(rgen(r)); res.adapter_generation = mr::AdapterGeneration(rgen(r));
  res.device_generation = mr::DeviceGeneration(rgen(r)); res.policy_generation = mr::PolicyGeneration(rgen(r));
  res.attempt = mr::AttemptId::from_bytes(r.uid().bytes());
  res.migration = mr::MigrationId::from_bytes(r.uid().bytes());
  res.eviction = mr::EvictionId::from_bytes(r.uid().bytes());
  res.eviction_reason = eviction_reason_from_int(r.u8()); res.failed_reason = r.string();
  res.handle = nullptr;
  return res;
}
void enc_ids(BinWriter& w, const std::vector<mr::ResidencyId>& ids) {
  w.u32(static_cast<std::uint32_t>(ids.size()));
  for (const auto& id : ids) { w.uid(id.value()); }
}
std::vector<mr::ResidencyId> dec_ids(BinReader& r) {
  const std::uint32_t n = count_checked(r);
  std::vector<mr::ResidencyId> ids;
  ids.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) { ids.push_back(mr::ResidencyId::from_bytes(r.uid().bytes())); }
  return ids;
}
void enc_set(BinWriter& w, const mr::ResidencySet& s) {
  w.uid(s.id.value()); w.uid(s.model.value()); w.uid(s.model_revision.value()); w.uid(s.adapter_set.value());
  wgen(w, s.generation.value()); enc_ids(w, s.required_members); enc_ids(w, s.optional_members); enc_ids(w, s.members);
  w.bytes(s.aggregate_bytes); w.boolean(s.migration_blocked); w.string(s.failure_reason);
  w.uid(s.migration.value()); w.boolean(s.ready); w.boolean(s.complete);
}
mr::ResidencySet dec_set(BinReader& r) {
  mr::ResidencySet s;
  s.id = mr::ResidencySetId::from_bytes(r.uid().bytes()); s.model = mr::ModelId::from_bytes(r.uid().bytes());
  s.model_revision = mr::ModelRevisionId::from_bytes(r.uid().bytes());
  s.adapter_set = mr::AdapterSetId::from_bytes(r.uid().bytes()); s.generation = mr::ResidencyGeneration(rgen(r));
  s.required_members = dec_ids(r); s.optional_members = dec_ids(r); s.members = dec_ids(r);
  s.aggregate_bytes = r.bytes(); s.migration_blocked = r.boolean(); s.failure_reason = r.string();
  s.migration = mr::MigrationId::from_bytes(r.uid().bytes()); s.ready = r.boolean(); s.complete = r.boolean();
  return s;
}

void validate_snapshot(const CoordinatorSnapshot& snap) {
  std::set<mr::Uid128> model_ids, resid_ids, domain_ids, set_ids;
  for (const auto& m : snap.models) {
    throw_if(!m.is_valid(), ErrorCode::PersistenceCorruption, "invalid model revision in payload");
    throw_if(!model_ids.insert(m.model.value()).second, ErrorCode::PersistenceCorruption, "duplicate model id");
  }
  for (const auto& d : snap.domains) {
    throw_if(d.id.is_nil(), ErrorCode::PersistenceCorruption, "nil domain id");
    throw_if(!domain_ids.insert(d.id.value()).second, ErrorCode::PersistenceCorruption, "duplicate domain id");
    d.verify_invariants();
  }
  for (const auto& r : snap.residencies) {
    if (!r.id.is_nil()) { throw_if(!resid_ids.insert(r.id.value()).second, ErrorCode::PersistenceCorruption, "duplicate residency id"); }
    throw_if(r.handle != nullptr, ErrorCode::PersistenceCorruption, "residency leaked a live handle");
  }
  for (const auto& s : snap.sets) {
    if (!s.id.is_nil()) { throw_if(!set_ids.insert(s.id.value()).second, ErrorCode::PersistenceCorruption, "duplicate set id"); }
    for (const auto& m : s.members) { throw_if(!resid_ids.count(m.value()), ErrorCode::PersistenceCorruption, "set member unknown residency"); }
    for (const auto& req : s.required_members) { throw_if(!resid_ids.count(req.value()), ErrorCode::PersistenceCorruption, "required member unknown residency"); }
  }
}

} // namespace

std::vector<std::uint8_t> PersistenceCodec::encode(const CoordinatorSnapshot& snap) {
  BinWriter payload;
  wgen(payload, snap.epoch.value()); wgen(payload, snap.policy_generation.value());
  payload.u32(static_cast<std::uint32_t>(snap.models.size()));
  for (const auto& m : snap.models) { enc_model(payload, m); }
  payload.u32(static_cast<std::uint32_t>(snap.adapter_sets.size()));
  for (const auto& s : snap.adapter_sets) { enc_adapterset(payload, s); }
  payload.u32(static_cast<std::uint32_t>(snap.domains.size()));
  for (const auto& d : snap.domains) { enc_domain(payload, d); }
  payload.u32(static_cast<std::uint32_t>(snap.residencies.size()));
  for (const auto& r : snap.residencies) { enc_residency(payload, r); }
  payload.u32(static_cast<std::uint32_t>(snap.sets.size()));
  for (const auto& s : snap.sets) { enc_set(payload, s); }
  std::vector<std::uint8_t> p = payload.take();
  BinWriter out;
  out.u64(kPersistenceMagic); out.u32(kPersistenceVersion); out.u64(static_cast<std::uint64_t>(p.size()));
  out.raw_bytes(p.data(), p.size()); out.u32(crc32(p.data(), p.size()));
  return out.take();
}

CoordinatorSnapshot PersistenceCodec::decode(std::span<const std::uint8_t> bytes) {
  BinReader r(bytes.data(), bytes.size());
  const std::uint64_t magic = r.u64();
  throw_if(magic != kPersistenceMagic, ErrorCode::PersistenceFormat, "bad persistence magic");
  const std::uint32_t version = r.u32();
  throw_if(version != kPersistenceVersion, ErrorCode::PersistenceFormat, "unsupported persistence version");
  const std::uint64_t plen = r.u64();
  throw_if(plen > r.remaining(), ErrorCode::PersistenceTruncation, "payload length exceeds available bytes");
  auto payload = r.raw_span(static_cast<std::size_t>(plen));
  const std::uint32_t stored_crc = r.u32();
  throw_if(stored_crc != crc32(payload.data(), payload.size()), ErrorCode::PersistenceChecksum, "checksum mismatch");
  throw_if(!r.at_end(), ErrorCode::PersistenceFormat, "trailing garbage after payload");
  BinReader pr(payload.data(), payload.size());
  CoordinatorSnapshot snap;
  snap.epoch = CoordinatorEpoch(rgen(pr)); snap.policy_generation = PolicyGeneration(rgen(pr));
  const std::uint32_t nm = count_checked(pr);
  for (std::uint32_t i = 0; i < nm; ++i) { snap.models.push_back(dec_model(pr)); }
  const std::uint32_t na = count_checked(pr);
  for (std::uint32_t i = 0; i < na; ++i) { snap.adapter_sets.push_back(dec_adapterset(pr)); }
  const std::uint32_t nd = count_checked(pr);
  for (std::uint32_t i = 0; i < nd; ++i) { snap.domains.push_back(dec_domain(pr)); }
  const std::uint32_t nr = count_checked(pr);
  for (std::uint32_t i = 0; i < nr; ++i) { snap.residencies.push_back(dec_residency(pr)); }
  const std::uint32_t ns = count_checked(pr);
  for (std::uint32_t i = 0; i < ns; ++i) { snap.sets.push_back(dec_set(pr)); }
  throw_if(!pr.at_end(), ErrorCode::PersistenceFormat, "trailing bytes in payload");
  validate_snapshot(snap);
  return snap;
}

} // namespace mr
