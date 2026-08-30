#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "mr/net.hpp"
#include "mr/protocol.hpp"

#ifdef _WIN32
#  define NOMINMAX
#  include <windows.h>
#endif

namespace {

using namespace mr;

class Proc {
 public:
  void spawn(const std::string& exe, const std::string& args) {
#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::string cmd = "\"" + exe + "\" " + args;
    const bool ok = CreateProcessA(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (ok) { h_ = pi.hProcess; CloseHandle(pi.hThread); }
#endif
  }
  ~Proc() { kill(); if (h_) { CloseHandle(h_); } }
  void kill() { if (h_) { TerminateProcess(h_, 0); } }
  bool alive() const {
#ifdef _WIN32
    return h_ != nullptr && WaitForSingleObject(h_, 0) == WAIT_TIMEOUT;
#else
    return false;
#endif
  }
 private:
  void* h_ = nullptr;
};

bool file_has_ready(const std::string& path) {
  std::ifstream in(path);
  std::string line;
  if (!std::getline(in, line)) { return false; }
  return line.find("ready") != std::string::npos;
}

struct FileGuard {
  std::string path;
  ~FileGuard() { std::remove(path.c_str()); }
};

void send_frame(TcpSocket& s, MsgKind kind, const std::vector<std::uint8_t>& payload) {
  const auto frame = FrameCodec::encode(kind, payload.data(), payload.size());
  if (!s.write_frame(frame.data(), frame.size())) { throw std::runtime_error("send failed"); }
}
std::pair<MsgKind, std::vector<std::uint8_t>> recv_frame(TcpSocket& s) {
  std::vector<std::uint8_t> frame;
  if (!s.read_frame(frame)) { throw std::runtime_error("recv failed"); }
  MsgKind kind; std::vector<std::uint8_t> payload;
  if (!FrameCodec::decode(frame, kind, payload)) { throw std::runtime_error("decode failed"); }
  return {kind, payload};
}
template <typename F>
void send_control(TcpSocket& s, MsgKind kind, F&& enc) {
  BinWriter w; enc(w); auto fb = w.take(); send_frame(s, kind, fb);
}
std::vector<std::uint8_t> roundtrip(TcpSocket& s, MsgKind kind, const std::vector<std::uint8_t>& payload) {
  send_frame(s, kind, payload);
  auto [k, p] = recv_frame(s);
  return p;
}

int fail(const char* msg) { std::cerr << "MULTIPROCESS PROOF FAILED: " << msg << std::endl; return 1; }

} // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::cerr << "usage: proof <coordinator_exe> <worker_exe>" << std::endl; return 1; }
  NetInit net;
  if (!net.ok()) { return fail("net init"); }
  const std::string coord_exe = argv[1];
  const std::string worker_exe = argv[2];
  const std::uint16_t port = 19100;

  FileGuard ready_f;
  ready_f.path = "mr_coord_ready.txt";
  std::remove(ready_f.path.c_str());

  Proc coord_proc;
  coord_proc.spawn(coord_exe, "--port " + std::to_string(port) + " --ready " + ready_f.path);
  bool coord_ready = false;
  for (int i = 0; i < 200 && !coord_ready; ++i) {
    if (!coord_proc.alive()) { return fail("coordinator exited before ready"); }
    if (file_has_ready(ready_f.path)) { coord_ready = true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if (!coord_ready) { return fail("coordinator never became ready"); }

  TcpSocket control = tcp_connect("127.0.0.1", port + 1);
  if (!control.valid()) { return fail("control connect failed"); }

  const NodeId nodeA = NodeId::from_hex("11111111111111111111111111111111");
  const NodeId nodeB = NodeId::from_hex("22222222222222222222222222222222");
  const DeviceId devA = DeviceId::from_hex("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const DeviceId devB = DeviceId::from_hex("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  const WorkerId widA = WorkerId::from_hex("33333333333333333333333333333333");
  const WorkerId widB = WorkerId::from_hex("44444444444444444444444444444444");
  const WorkerBootId bootA = WorkerBootId::from_hex("55555555555555555555555555555555");
  const WorkerBootId bootB = WorkerBootId::from_hex("66666666666666666666666666666666");
  const WorkerBootId bootA2 = WorkerBootId::from_hex("77777777777777777777777777777777");

  Proc procA;
  procA.spawn(worker_exe, "--host 127.0.0.1 --port " + std::to_string(port) +
              " --worker " + widA.to_hex() + " --boot " + bootA.to_hex() +
              " --node " + nodeA.to_hex() + " --device " + devA.to_hex());
  Proc procB;
  procB.spawn(worker_exe, "--host 127.0.0.1 --port " + std::to_string(port) +
              " --worker " + widB.to_hex() + " --boot " + bootB.to_hex() +
              " --node " + nodeB.to_hex() + " --device " + devB.to_hex());

  const ModelId model = ModelId::from_hex("88888888888888888888888888888888");
  const ModelRevisionId rev = ModelRevisionId::from_hex("99999999999999999999999999999999");
  const MemoryDomainId dom = MemoryDomainId::from_hex("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1");
  const std::uint64_t bytes = 512;

  send_control(control, MsgKind::RegisterModel, [&](BinWriter& w) {
    RegisterModelMsg m; m.model = model; m.revision = rev; m.required_bytes = bytes;
    encode_register_model(w, m);
  });
  (void)recv_frame(control);
  send_control(control, MsgKind::RegisterDomain, [&](BinWriter& w) {
    RegisterDomainMsg m; m.domain = dom; m.device = devA; m.node = nodeA; m.governed_bytes = (1u << 20);
    encode_register_domain(w, m);
  });
  (void)recv_frame(control);

  send_control(control, MsgKind::ControlLoad, [&](BinWriter& w) {
    ControlLoadMsg m; m.model = model; m.revision = rev; encode_control_load(w, m);
  });
  auto lp = recv_frame(control);
  BinReader lr(lp.second.data(), lp.second.size());
  ControlLoadReplyMsg reply = decode_control_load_reply(lr);
  if (!reply.success) { return fail(("first load failed: " + reply.error).c_str()); }
  std::cout << "PASS first load on worker A (residency ready)" << std::endl;

  procA.kill();
  bool saw_loss = false;
  for (int i = 0; i < 300 && !saw_loss; ++i) {
    auto sp = roundtrip(control, MsgKind::ControlState, {});
    const std::string st(reinterpret_cast<const char*>(sp.data()), sp.size());
    if (st.find("\"workers_connected\":1") != std::string::npos) { saw_loss = true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (!saw_loss) { return fail("coordinator did not observe worker A loss"); }
  std::cout << "PASS coordinator observed worker A loss and rolled authority" << std::endl;

  procA.spawn(worker_exe, "--host 127.0.0.1 --port " + std::to_string(port) +
              " --worker " + widA.to_hex() + " --boot " + bootA2.to_hex() +
              " --node " + nodeA.to_hex() + " --device " + devA.to_hex());
  bool back2 = false;
  for (int i = 0; i < 300 && !back2; ++i) {
    auto sp = roundtrip(control, MsgKind::ControlState, {});
    const std::string st(reinterpret_cast<const char*>(sp.data()), sp.size());
    if (st.find("\"workers_connected\":2") != std::string::npos) { back2 = true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (!back2) { return fail("worker A did not re-register"); }
  std::cout << "PASS worker A restarted under a fresh WorkerBootId" << std::endl;

  TcpSocket stale = tcp_connect("127.0.0.1", port);
  if (!stale.valid()) { return fail("stale conn failed"); }
  {
    BinWriter w; RegisterWorkerMsg m;
    m.worker = widA; m.boot = bootA; m.node = nodeA; m.protocol_version = 1;
    m.devices = {devA}; m.device_generations = {DeviceGeneration::first()};
    encode_register_worker(w, m);
    auto fb = w.take();
    send_frame(stale, MsgKind::RegisterWorker, fb);
  }
  std::vector<std::uint8_t> doomed;
  const bool got_reply = stale.read_frame(doomed);
  if (got_reply) { return fail("stale boot registration was NOT rejected"); }
  std::cout << "PASS stale boot registration rejected" << std::endl;

  send_control(control, MsgKind::ControlLoad, [&](BinWriter& w) {
    ControlLoadMsg m; m.model = model; m.revision = rev; encode_control_load(w, m);
  });
  auto lp2 = recv_frame(control);
  BinReader lr2(lp2.second.data(), lp2.second.size());
  ControlLoadReplyMsg reply2 = decode_control_load_reply(lr2);
  if (!reply2.success) { return fail(("fresh load failed: " + reply2.error).c_str()); }

  const auto sp2 = roundtrip(control, MsgKind::ControlState, {});
  const std::string st2(reinterpret_cast<const char*>(sp2.data()), sp2.size());
  if (st2.find("\"lifecycle\":\"READY\"") == std::string::npos) { return fail("no READY residency after fresh load"); }
  if (st2.find("\"authority\":\"authoritative\"") == std::string::npos) { return fail("residency not authoritative"); }
  if (st2.find("\"stale_rejected\":") == std::string::npos) { return fail("state missing stale_rejected"); }
  const auto pos = st2.find("\"stale_rejected\":");
  const int v = std::stoi(st2.substr(pos + 17));
  if (v < 1) { return fail("stale_rejected counter is zero"); }
  std::cout << "PASS fresh work under new authority; no duplicate residency; stale rejected" << std::endl;

  std::cout << "MULTIPROCESS RESTART PROOF PASSED" << std::endl;
  return 0;
}
