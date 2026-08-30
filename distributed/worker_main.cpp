#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "mr/backends/memory_backend.hpp"
#include "mr/net.hpp"
#include "mr/protocol.hpp"

namespace {

using namespace mr;

// Args: --host H --port P --worker WIDHEX --boot BIDHEX --node NIDHEX --device DIDHEX
std::string arg_value(int argc, char** argv, const std::string& name) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = std::string(argv[i]);
    if (a == name && i + 1 < argc) { return std::string(argv[i + 1]); }
  }
  return {};
}

} // namespace

int main(int argc, char** argv) {
  NetInit net;
  if (!net.ok()) { return 1; }
  const std::string host = arg_value(argc, argv, "--host");
  const std::string port_s = arg_value(argc, argv, "--port");
  const std::uint16_t port = static_cast<std::uint16_t>(std::stoi(port_s));
  const WorkerId worker = WorkerId::from_hex(arg_value(argc, argv, "--worker"));
  const WorkerBootId boot = WorkerBootId::from_hex(arg_value(argc, argv, "--boot"));
  const NodeId node = NodeId::from_hex(arg_value(argc, argv, "--node"));
  const DeviceId device = DeviceId::from_hex(arg_value(argc, argv, "--device"));

  TcpSocket s = tcp_connect(host, port);
  if (!s.valid()) { std::cerr << "worker connect failed\n"; return 1; }

  // Register.
  RegisterWorkerMsg reg;
  reg.worker = worker; reg.boot = boot; reg.node = node;
  reg.protocol_version = 1; reg.devices = {device};
  reg.device_generations = {DeviceGeneration::first()};
  {
    BinWriter w; encode_register_worker(w, reg); auto fb = w.take();
    auto frame = FrameCodec::encode(MsgKind::RegisterWorker, fb.data(), fb.size());
    s.write_frame(frame.data(), frame.size());
  }
  // Read epoch.
  std::vector<std::uint8_t> frame;
  if (!s.read_frame(frame)) { return 1; }
  MsgKind kind; std::vector<std::uint8_t> payload;
  FrameCodec::decode(frame, kind, payload);
  if (kind != MsgKind::WorkerRegistered) { return 1; }

  MemoryBackend backend;
  std::map<ResidencyId, void*> handles;
  std::cout << "WORKER_READY " << worker.to_hex() << std::endl;

  while (s.read_frame(frame)) {
    if (!FrameCodec::decode(frame, kind, payload)) { break; }
    BinReader r(payload.data(), payload.size());
    if (kind == MsgKind::CoordLoad) {
      CoordLoadMsg m;
      try { m = decode_coord_load(r); } catch (const std::exception&) { continue; }
      ModelRevision mrev;
      mrev.model = m.model; mrev.revision = m.revision; mrev.version = "1.0";
      mrev.artifact.id = ArtifactId::random(); mrev.artifact.generation = ArtifactGeneration::first();
      mrev.artifact.backend = BackendType::Cuda; mrev.artifact.capability = ComputeCapability::Sm_120;
      mrev.backend_compat = {BackendType::Cuda}; mrev.device_compat = {ComputeCapability::Sm_120};
      mrev.required_memory = Bytes(m.required_bytes); mrev.required_device_memory = Bytes(m.required_bytes);
      LoadTarget target;
      target.domain = m.domain; target.kind = MemoryDomainKind::Accelerator; target.cls = ResidencyClass::AcceleratorLocal;
      target.device = m.device; target.node = node; target.backend = BackendType::Cuda;
      LoadResult lr = backend.load(target, mrev, nullptr);
      const ResidencyId resid = ResidencyId::random();
      handles[resid] = lr.handle;
      WorkerLoadResultMsg res;
      res.success = lr.success; res.residency = resid; res.bytes = lr.loaded.value(); res.error = lr.error;
      BinWriter w; encode_worker_load_result(w, res); auto fb = w.take();
      auto out = FrameCodec::encode(MsgKind::WorkerLoadResult, fb.data(), fb.size());
      s.write_frame(out.data(), out.size());
    } else if (kind == MsgKind::CoordEvict) {
      CoordEvictMsg m;
      try { m = decode_coord_evict(r); } catch (const std::exception&) { continue; }
      auto it = handles.find(m.residency);
      if (it != handles.end()) { backend.evict(ResidencyClass::AcceleratorLocal, it->second); handles.erase(it); }
      WorkerEvictResultMsg res; res.success = true;
      BinWriter w; encode_worker_evict_result(w, res); auto fb = w.take();
      auto out = FrameCodec::encode(MsgKind::WorkerEvictResult, fb.data(), fb.size());
      s.write_frame(out.data(), out.size());
    }
  }
  // Worker exit -> coordinator detects socket close.
  return 0;
}
