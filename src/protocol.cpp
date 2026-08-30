#include "mr/protocol.hpp"

#include <cstring>

#include "mr/core/error.hpp"

namespace mr {
namespace {

void put_uid(BinWriter& w, const Uid128& v) { w.uid(v); }
Uid128 take_uid(BinReader& r) { return r.uid(); }

} // namespace

std::vector<std::uint8_t> FrameCodec::encode(MsgKind kind, const std::uint8_t* payload, std::size_t size) {
  BinWriter w;
  w.u32(static_cast<std::uint32_t>(kind));
  if (size > 0) { w.raw_bytes(payload, size); }
  return w.take();
}

bool FrameCodec::decode(std::span<const std::uint8_t> frame, MsgKind& kind, std::vector<std::uint8_t>& payload) {
  if (frame.size() < 4) { return false; }
  BinReader r(frame.data(), frame.size());
  const std::uint32_t k = r.u32();
  kind = static_cast<MsgKind>(k);
  payload.assign(frame.begin() + 4, frame.end());
  return true;
}

void encode_register_worker(BinWriter& w, const RegisterWorkerMsg& m) {
  put_uid(w, m.worker.value()); put_uid(w, m.boot.value()); put_uid(w, m.node.value());
  w.u32(m.protocol_version);
  w.u32(static_cast<std::uint32_t>(m.devices.size()));
  for (const auto& d : m.devices) { put_uid(w, d.value()); }
  w.u32(static_cast<std::uint32_t>(m.device_generations.size()));
  for (const auto& g : m.device_generations) { w.u64(g.value()); }
}
RegisterWorkerMsg decode_register_worker(BinReader& r) {
  RegisterWorkerMsg m;
  m.worker = WorkerId::from_bytes(take_uid(r).bytes());
  m.boot = WorkerBootId::from_bytes(take_uid(r).bytes());
  m.node = NodeId::from_bytes(take_uid(r).bytes());
  m.protocol_version = r.u32();
  const std::uint32_t nd = r.u32();
  if (nd > (1u << 20)) { throw Error(ErrorCode::ProtocolMalformed, "too many devices"); }
  for (std::uint32_t i = 0; i < nd; ++i) { m.devices.push_back(DeviceId::from_bytes(take_uid(r).bytes())); }
  const std::uint32_t ng = r.u32();
  if (ng > (1u << 20)) { throw Error(ErrorCode::ProtocolMalformed, "too many device gens"); }
  for (std::uint32_t i = 0; i < ng; ++i) { m.device_generations.push_back(DeviceGeneration(r.u64())); }
  return m;
}
void encode_worker_registered(BinWriter& w, const WorkerRegisteredMsg& m) {
  w.u64(m.epoch.value()); w.string(m.error);
}
WorkerRegisteredMsg decode_worker_registered(BinReader& r) {
  WorkerRegisteredMsg m;
  m.epoch = CoordinatorEpoch(r.u64()); m.error = r.string();
  return m;
}
void encode_worker_hello(BinWriter& w, const WorkerHelloMsg& m) {
  put_uid(w, m.worker.value()); put_uid(w, m.boot.value());
}
WorkerHelloMsg decode_worker_hello(BinReader& r) {
  WorkerHelloMsg m;
  m.worker = WorkerId::from_bytes(take_uid(r).bytes());
  m.boot = WorkerBootId::from_bytes(take_uid(r).bytes());
  return m;
}
void encode_worker_report(BinWriter& w, const WorkerReportMsg& m) {
  put_uid(w, m.worker.value()); put_uid(w, m.boot.value()); put_uid(w, m.residency.value());
  w.u8(m.lifecycle); w.u8(m.validity); w.u8(m.readiness);
  w.u64(m.generation.value()); w.u64(m.model_generation.value()); w.u64(m.artifact_generation.value());
  w.u64(m.adapter_generation.value()); w.u64(m.device_generation.value()); w.u64(m.epoch.value());
}
WorkerReportMsg decode_worker_report(BinReader& r) {
  WorkerReportMsg m;
  m.worker = WorkerId::from_bytes(take_uid(r).bytes());
  m.boot = WorkerBootId::from_bytes(take_uid(r).bytes());
  m.residency = ResidencyId::from_bytes(take_uid(r).bytes());
  m.lifecycle = r.u8(); m.validity = r.u8(); m.readiness = r.u8();
  m.generation = ResidencyGeneration(r.u64()); m.model_generation = ModelGeneration(r.u64());
  m.artifact_generation = ArtifactGeneration(r.u64());
  m.adapter_generation = AdapterGeneration(r.u64());
  m.device_generation = DeviceGeneration(r.u64());
  m.epoch = CoordinatorEpoch(r.u64());
  return m;
}
void encode_coord_load(BinWriter& w, const CoordLoadMsg& m) {
  put_uid(w, m.model.value()); put_uid(w, m.revision.value()); put_uid(w, m.domain.value()); put_uid(w, m.device.value());
  w.u64(m.required_bytes);
}
CoordLoadMsg decode_coord_load(BinReader& r) {
  CoordLoadMsg m;
  m.model = ModelId::from_bytes(take_uid(r).bytes());
  m.revision = ModelRevisionId::from_bytes(take_uid(r).bytes());
  m.domain = MemoryDomainId::from_bytes(take_uid(r).bytes());
  m.device = DeviceId::from_bytes(take_uid(r).bytes());
  m.required_bytes = r.u64();
  return m;
}
void encode_worker_load_result(BinWriter& w, const WorkerLoadResultMsg& m) {
  w.boolean(m.success); put_uid(w, m.residency.value()); w.u64(m.bytes); w.string(m.error);
}
WorkerLoadResultMsg decode_worker_load_result(BinReader& r) {
  WorkerLoadResultMsg m;
  m.success = r.boolean(); m.residency = ResidencyId::from_bytes(take_uid(r).bytes());
  m.bytes = r.u64(); m.error = r.string();
  return m;
}
void encode_coord_evict(BinWriter& w, const CoordEvictMsg& m) {
  put_uid(w, m.residency.value());
}
CoordEvictMsg decode_coord_evict(BinReader& r) {
  CoordEvictMsg m;
  m.residency = ResidencyId::from_bytes(take_uid(r).bytes());
  return m;
}
void encode_worker_evict_result(BinWriter& w, const WorkerEvictResultMsg& m) {
  w.boolean(m.success); w.string(m.error);
}
WorkerEvictResultMsg decode_worker_evict_result(BinReader& r) {
  WorkerEvictResultMsg m;
  m.success = r.boolean(); m.error = r.string();
  return m;
}
void encode_register_model(BinWriter& w, const RegisterModelMsg& m) {
  put_uid(w, m.model.value()); put_uid(w, m.revision.value()); w.u64(m.required_bytes);
}
RegisterModelMsg decode_register_model(BinReader& r) {
  RegisterModelMsg m;
  m.model = ModelId::from_bytes(take_uid(r).bytes());
  m.revision = ModelRevisionId::from_bytes(take_uid(r).bytes());
  m.required_bytes = r.u64();
  return m;
}
void encode_register_domain(BinWriter& w, const RegisterDomainMsg& m) {
  put_uid(w, m.domain.value()); put_uid(w, m.device.value()); put_uid(w, m.node.value()); w.u64(m.governed_bytes);
}
RegisterDomainMsg decode_register_domain(BinReader& r) {
  RegisterDomainMsg m;
  m.domain = MemoryDomainId::from_bytes(take_uid(r).bytes());
  m.device = DeviceId::from_bytes(take_uid(r).bytes());
  m.node = NodeId::from_bytes(take_uid(r).bytes());
  m.governed_bytes = r.u64();
  return m;
}
void encode_control_load(BinWriter& w, const ControlLoadMsg& m) {
  put_uid(w, m.model.value()); put_uid(w, m.revision.value());
}
ControlLoadMsg decode_control_load(BinReader& r) {
  ControlLoadMsg m;
  m.model = ModelId::from_bytes(take_uid(r).bytes());
  m.revision = ModelRevisionId::from_bytes(take_uid(r).bytes());
  return m;
}
void encode_control_load_reply(BinWriter& w, const ControlLoadReplyMsg& m) {
  w.boolean(m.success); put_uid(w, m.residency.value()); w.u64(m.bytes); w.string(m.error);
}
ControlLoadReplyMsg decode_control_load_reply(BinReader& r) {
  ControlLoadReplyMsg m;
  m.success = r.boolean(); m.residency = ResidencyId::from_bytes(take_uid(r).bytes());
  m.bytes = r.u64(); m.error = r.string();
  return m;
}

} // namespace mr
