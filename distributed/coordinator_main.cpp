#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"
#include "mr/net.hpp"
#include "mr/protocol.hpp"

namespace {

using namespace mr;

class CoordinatorServer {
 public:
  explicit CoordinatorServer(std::uint16_t port) : port_(port) {}

  bool start() {
    if (!worker_listener_.listen("127.0.0.1", port_)) { return false; }
    if (!control_listener_.listen("127.0.0.1", port_ + 1)) { return false; }
    worker_thread_ = std::thread(&CoordinatorServer::worker_loop, this);
    control_thread_ = std::thread(&CoordinatorServer::control_loop, this);
    return true;
  }
  void stop() {
    running_ = false;
    {
      std::lock_guard<std::mutex> l(conns_mx_);
      for (auto& [id, c] : conns_) { if (c->sock.valid()) { c->sock.close(); } }
    }
    if (worker_thread_.joinable()) { worker_thread_.join(); }
    if (control_thread_.joinable()) { control_thread_.join(); }
  }

  int stale_rejected() const { std::lock_guard<std::mutex> l(stats_mx_); return stale_rejected_; }
  Coordinator& coordinator() { return coord_; }

 private:
  struct WorkerConn {
    TcpSocket sock;
    WorkerId id;
    WorkerBootId boot;
    DeviceId device;
    bool alive = true;
  };

  void worker_loop() {
    while (running_) {
      TcpSocket s = worker_listener_.accept();
      if (!s.valid()) { continue; }
      std::thread([this, s = std::move(s)]() mutable { worker_conn(std::move(s)); }).detach();
    }
  }
  void control_loop() {
    while (running_) {
      TcpSocket s = control_listener_.accept();
      if (!s.valid()) { continue; }
      std::thread([this, s = std::move(s)]() mutable { control_conn(std::move(s)); }).detach();
    }
  }

  void worker_conn(TcpSocket sock) {
    auto conn = std::make_shared<WorkerConn>();
    conn->sock = std::move(sock);
    std::vector<std::uint8_t> frame;
    if (!conn->sock.read_frame(frame)) { return; }
    MsgKind kind; std::vector<std::uint8_t> payload;
    if (!FrameCodec::decode(frame, kind, payload)) { return; }
    if (kind != MsgKind::RegisterWorker) { return; }
    BinReader r(payload.data(), payload.size());
    RegisterWorkerMsg reg;
    try { reg = decode_register_worker(r); } catch (const std::exception&) { return; }
    if (reg.worker.is_nil() || reg.boot.is_nil() || reg.devices.empty()) { return; }
    conn->id = reg.worker; conn->boot = reg.boot; conn->device = reg.devices.front();
    {
      std::lock_guard<std::mutex> l(conns_mx_);
      auto ex = conns_.find(reg.worker);
      if (ex != conns_.end() && ex->second->alive && ex->second->boot != reg.boot) {
        // A worker id is already registered with a different, live boot id: this
        // is a stale replay -> reject the connection without registering.
        ++stale_rejected_;
        return;
      }
      conns_[reg.worker] = conn;
      current_boot_[reg.worker] = reg.boot;
    }
    WorkerRegistration wr;
    wr.worker = reg.worker; wr.boot = reg.boot; wr.node = reg.node;
    wr.protocol_version = reg.protocol_version; wr.devices = reg.devices;
    wr.device_generations = reg.device_generations;
    coord_.register_worker(wr);

    WorkerRegisteredMsg rm;
    rm.epoch = coord_.epoch();
    BinWriter w; encode_worker_registered(w, rm);
    auto fb = w.take();
    auto out = FrameCodec::encode(MsgKind::WorkerRegistered, fb.data(), fb.size());
    conn->sock.write_frame(out.data(), out.size());

    while (runtime_ok() && conn->sock.read_frame(frame)) {
      if (!FrameCodec::decode(frame, kind, payload)) { break; }
      try {
        BinReader rr(payload.data(), payload.size());
        if (kind == MsgKind::WorkerReport) {
          WorkerReportMsg rep = decode_worker_report(rr);
          handle_worker_report(*conn, rep);
        }
      } catch (const std::exception&) { break; }
    }
    conn->alive = false;
    {
      std::lock_guard<std::mutex> l(conns_mx_);
      conns_.erase(conn->id);
    }
    coord_.mark_worker_lost(conn->id, AuthorityFrame{});
  }

  void handle_worker_report(WorkerConn& conn, const WorkerReportMsg& rep) {
    WorkerBootId current_boot;
    {
      std::lock_guard<std::mutex> l(conns_mx_);
      auto it = current_boot_.find(conn.id);
      if (it != current_boot_.end()) { current_boot = it->second; }
    }
    AuthorityFrame reported;
    reported.coordinator_epoch = rep.epoch;
    reported.worker_boot = rep.boot;
    reported.residency_generation = rep.generation;
    reported.model_generation = rep.model_generation;
    reported.artifact_generation = rep.artifact_generation;
    reported.adapter_generation = rep.adapter_generation;
    reported.device_generation = rep.device_generation;

    AuthorityFrame current;
    current.coordinator_epoch = coord_.epoch();
    current.worker_boot = current_boot;
    current.residency_generation = rep.generation;
    current.model_generation = rep.model_generation;
    current.artifact_generation = rep.artifact_generation;
    current.adapter_generation = rep.adapter_generation;
    current.device_generation = rep.device_generation;

    StaleAuthorityChecker chk;
    if (chk.validate(reported, current, false) != StaleReason::None) {
      std::lock_guard<std::mutex> l(stats_mx_);
      ++stale_rejected_;
    }
  }

  void control_conn(TcpSocket sock) {
    std::vector<std::uint8_t> frame;
    while (runtime_ok() && sock.read_frame(frame)) {
      MsgKind kind; std::vector<std::uint8_t> payload;
      if (!FrameCodec::decode(frame, kind, payload)) { break; }
      try {
        BinReader r(payload.data(), payload.size());
        std::vector<std::uint8_t> response;
        if (kind == MsgKind::RegisterModel) {
          RegisterModelMsg m = decode_register_model(r);
          ModelRevision mrev;
          mrev.model = m.model; mrev.revision = m.revision; mrev.version = "1.0";
          mrev.artifact.id = ArtifactId::random(); mrev.artifact.generation = ArtifactGeneration::first();
          mrev.artifact.backend = BackendType::Cuda; mrev.artifact.capability = ComputeCapability::Sm_120;
          mrev.backend_compat = {BackendType::Cuda}; mrev.device_compat = {ComputeCapability::Sm_120};
          mrev.required_memory = Bytes(m.required_bytes); mrev.required_device_memory = Bytes(m.required_bytes);
          coord_.define_model(std::move(mrev));
          BinWriter w; encode_worker_registered(w, WorkerRegisteredMsg{});
          auto fb0 = w.take();
          response = FrameCodec::encode(MsgKind::WorkerRegistered, fb0.data(), fb0.size());
        } else if (kind == MsgKind::RegisterDomain) {
          RegisterDomainMsg m = decode_register_domain(r);
          MemoryDomain d;
          d.id = m.domain; d.kind = MemoryDomainKind::Accelerator; d.device = m.device;
          d.node = m.node; d.backend = BackendType::Cuda; d.capability = ComputeCapability::Sm_120;
          d.total_capacity = Bytes(m.governed_bytes); d.governed_capacity = Bytes(m.governed_bytes);
          coord_.register_domain(std::move(d));
          BinWriter w; encode_worker_registered(w, WorkerRegisteredMsg{});
          auto fb0 = w.take();
          response = FrameCodec::encode(MsgKind::WorkerRegistered, fb0.data(), fb0.size());
        } else if (kind == MsgKind::ControlLoad) {
          ControlLoadMsg m = decode_control_load(r);
          AdmitRequest req; req.model = m.model; req.model_revision = m.revision;
          LoadOutcome o = coord_.load(req, AuthorityFrame{});
          ControlLoadReplyMsg rep;
          rep.success = o.success; rep.residency = o.residency; rep.bytes = o.loaded.value(); rep.error = o.error;
          BinWriter w; encode_control_load_reply(w, rep);
          auto fb = w.take();
          response = FrameCodec::encode(MsgKind::ControlLoadReply, fb.data(), fb.size());
        } else if (kind == MsgKind::ControlState) {
          Json st = coord_.to_json();
          {
            int stale; std::size_t workers;
            {
              std::lock_guard<std::mutex> l(stats_mx_);
              stale = stale_rejected_;
            }
            {
              std::lock_guard<std::mutex> l(conns_mx_);
              workers = conns_.size();
            }
            st.set("stale_rejected", stale);
            st.set("workers_connected", static_cast<std::uint64_t>(workers));
          }
          const std::string ds = st.dump();
          response = FrameCodec::encode(MsgKind::ControlStateReply,
                                        reinterpret_cast<const std::uint8_t*>(ds.data()), ds.size());
        }
        if (!response.empty()) { sock.write_frame(response.data(), response.size()); }
      } catch (const std::exception&) { break; }
    }
  }

  bool runtime_ok() { return running_.load(); }

  std::uint16_t port_;
  TcpListener worker_listener_;
  TcpListener control_listener_;
  std::thread worker_thread_;
  std::thread control_thread_;
  std::atomic<bool> running_{true};

  MemoryBackend backend_;
  Coordinator coord_{&backend_, ResidencyPolicy{}};

  mutable std::mutex conns_mx_;
  std::map<WorkerId, std::shared_ptr<WorkerConn>> conns_;
  std::map<WorkerId, WorkerBootId> current_boot_;
  mutable std::mutex stats_mx_;
  int stale_rejected_ = 0;
};

} // namespace

std::string arg_value(int argc, char** argv, const std::string& name) {
  for (int i = 1; i < argc; ++i) {
    if (name == std::string(argv[i]) && i + 1 < argc) { return std::string(argv[i + 1]); }
  }
  return {};
}

int main(int argc, char** argv) {
  NetInit net;
  if (!net.ok()) { std::cerr << "network init failed\n"; return 1; }
  std::uint16_t port = 19010;
  const std::string port_s = arg_value(argc, argv, "--port");
  if (!port_s.empty()) { port = static_cast<std::uint16_t>(std::stoi(port_s)); }
  const std::string ready_file = arg_value(argc, argv, "--ready");

  CoordinatorServer server(port);
  if (!server.start()) { std::cerr << "server start failed\n"; return 1; }
  if (!ready_file.empty()) {
    std::ofstream os(ready_file, std::ios::trunc);
    os << "ready\n";
  }
  std::cout << "COORDINATOR_READY " << port << " " << (port + 1) << "\n" << std::flush;
  // Run until the proof driver terminates this process.
  for (;;) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
  return 0;
}