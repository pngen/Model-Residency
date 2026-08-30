#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/generation.hpp"
#include "mr/core/identity.hpp"
#include "mr/core/serialization.hpp"

namespace mr {

// ===========================================================================
// Framed protocol message kinds
// ===========================================================================
enum class MsgKind : std::uint32_t {
  RegisterWorker = 1,
  WorkerRegistered = 2,
  WorkerReport = 3,
  CoordLoad = 4,
  WorkerLoadResult = 5,
  CoordEvict = 6,
  WorkerEvictResult = 7,
  CoordShutdown = 8,
  RegisterModel = 9,
  RegisterDomain = 10,
  ControlLoad = 11,
  ControlLoadReply = 12,
  ControlState = 13,
  ControlStateReply = 14,
  WorkerHello = 15,
};

// ===========================================================================
// Message payloads
// ===========================================================================
struct RegisterWorkerMsg {
  WorkerId worker = WorkerId::nil();
  WorkerBootId boot = WorkerBootId::nil();
  NodeId node = NodeId::nil();
  std::uint32_t protocol_version = 1;
  std::vector<DeviceId> devices;
  std::vector<DeviceGeneration> device_generations;
};

struct WorkerRegisteredMsg {
  CoordinatorEpoch epoch = CoordinatorEpoch::zero();
  std::string error;
};

struct WorkerHelloMsg {
  WorkerId worker = WorkerId::nil();
  WorkerBootId boot = WorkerBootId::nil();
};

struct WorkerReportMsg {
  WorkerId worker = WorkerId::nil();
  WorkerBootId boot = WorkerBootId::nil();
  ResidencyId residency = ResidencyId::nil();
  std::uint8_t lifecycle = 0;
  std::uint8_t validity = 0;
  std::uint8_t readiness = 0;
  ResidencyGeneration generation = ResidencyGeneration::zero();
  ModelGeneration model_generation = ModelGeneration::zero();
  ArtifactGeneration artifact_generation = ArtifactGeneration::zero();
  AdapterGeneration adapter_generation = AdapterGeneration::zero();
  DeviceGeneration device_generation = DeviceGeneration::zero();
  CoordinatorEpoch epoch = CoordinatorEpoch::zero();
};

struct CoordLoadMsg {
  ModelId model = ModelId::nil();
  ModelRevisionId revision = ModelRevisionId::nil();
  MemoryDomainId domain = MemoryDomainId::nil();
  DeviceId device = DeviceId::nil();
  std::uint64_t required_bytes = 0;
};

struct WorkerLoadResultMsg {
  bool success = false;
  ResidencyId residency = ResidencyId::nil();
  std::uint64_t bytes = 0;
  std::string error;
};

struct CoordEvictMsg {
  ResidencyId residency = ResidencyId::nil();
};

struct WorkerEvictResultMsg {
  bool success = false;
  std::string error;
};

struct RegisterModelMsg {
  ModelId model = ModelId::nil();
  ModelRevisionId revision = ModelRevisionId::nil();
  std::uint64_t required_bytes = 0; // monolithic
};

struct RegisterDomainMsg {
  MemoryDomainId domain = MemoryDomainId::nil();
  DeviceId device = DeviceId::nil();
  NodeId node = NodeId::nil();
  std::uint64_t governed_bytes = 0;
};

struct ControlLoadMsg {
  ModelId model = ModelId::nil();
  ModelRevisionId revision = ModelRevisionId::nil();
};

struct ControlLoadReplyMsg {
  bool success = false;
  ResidencyId residency = ResidencyId::nil();
  std::uint64_t bytes = 0;
  std::string error;
};

// ===========================================================================
// Frame codec
// ===========================================================================
/// A single framed message: one MsgKind followed by its payload bytes.
/// Encode/decode uses the bounded BinWriter/BinReader, so a malformed frame is
/// rejected deterministically.
class FrameCodec {
 public:
  static std::vector<std::uint8_t> encode(MsgKind kind, const std::uint8_t* payload, std::size_t size);
  static bool decode(std::span<const std::uint8_t> frame, MsgKind& kind,
                     std::vector<std::uint8_t>& payload);
};

// Payload codecs.
void encode_register_worker(BinWriter& w, const RegisterWorkerMsg& m);
RegisterWorkerMsg decode_register_worker(BinReader& r);
void encode_worker_registered(BinWriter& w, const WorkerRegisteredMsg& m);
WorkerRegisteredMsg decode_worker_registered(BinReader& r);
void encode_worker_hello(BinWriter& w, const WorkerHelloMsg& m);
WorkerHelloMsg decode_worker_hello(BinReader& r);
void encode_worker_report(BinWriter& w, const WorkerReportMsg& m);
WorkerReportMsg decode_worker_report(BinReader& r);
void encode_coord_load(BinWriter& w, const CoordLoadMsg& m);
CoordLoadMsg decode_coord_load(BinReader& r);
void encode_worker_load_result(BinWriter& w, const WorkerLoadResultMsg& m);
WorkerLoadResultMsg decode_worker_load_result(BinReader& r);
void encode_coord_evict(BinWriter& w, const CoordEvictMsg& m);
CoordEvictMsg decode_coord_evict(BinReader& r);
void encode_worker_evict_result(BinWriter& w, const WorkerEvictResultMsg& m);
WorkerEvictResultMsg decode_worker_evict_result(BinReader& r);
void encode_register_model(BinWriter& w, const RegisterModelMsg& m);
RegisterModelMsg decode_register_model(BinReader& r);
void encode_register_domain(BinWriter& w, const RegisterDomainMsg& m);
RegisterDomainMsg decode_register_domain(BinReader& r);
void encode_control_load(BinWriter& w, const ControlLoadMsg& m);
ControlLoadMsg decode_control_load(BinReader& r);
void encode_control_load_reply(BinWriter& w, const ControlLoadReplyMsg& m);
ControlLoadReplyMsg decode_control_load_reply(BinReader& r);

} // namespace mr
