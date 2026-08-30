#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "mr/backends/memory_backend.hpp"
#include "mr/coordinator.hpp"
#include "mr/core/json.hpp"

namespace {

using namespace mr;

int help() {
  std::cout << "Model Residency CLI\n"
            << "  list models | list residencies | list sets | inspect state\n"
            << "  inspect capacity | load | evict <id> | pin <id> | unpin <id>\n"
            << "  invalidate <id> | rebalance | roll | snapshot\n"
            << "  save <file> | recover <file> | run-demo\n"
            << "  migrate <id> <pinned|pageable> | demote <id>\n"
            << "  [--json] emits machine-readable output\n";
  return 0;
}

void out(const Json& j, bool json, const std::string& text) {
  if (json) { std::cout << j.dump() << std::endl; }
  else { std::cout << text << std::endl; }
}

int fatal(const std::exception& e) { std::cerr << "error: " << e.what() << std::endl; return 1; }

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) { return help(); }
  std::vector<std::string> args(argv + 1, argv + argc);
  const bool json = std::find(args.begin(), args.end(), "--json") != args.end();

  MemoryBackend backend;
  Coordinator coord(&backend, ResidencyPolicy{});
  const ModelId model = ModelId::from_hex("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const std::uint64_t bytes = 4096;

  // Seed a default model + domain so the CLI is immediately usable.
  ModelRevision mrev;
  mrev.model = model; mrev.revision = ModelRevisionId::random(); mrev.version = "1.0";
  mrev.artifact.id = ArtifactId::random(); mrev.artifact.generation = ArtifactGeneration::first();
  mrev.artifact.backend = BackendType::Cuda; mrev.artifact.capability = ComputeCapability::Sm_120;
  mrev.backend_compat = {BackendType::Cuda}; mrev.device_compat = {ComputeCapability::Sm_120};
  mrev.required_memory = Bytes(bytes); mrev.required_device_memory = Bytes(bytes);
  coord.define_model(std::move(mrev));
  MemoryDomain dom;
  dom.id = MemoryDomainId::random(); dom.kind = MemoryDomainKind::Accelerator;
  dom.device = DeviceId::random(); dom.backend = BackendType::Cuda; dom.capability = ComputeCapability::Sm_120;
  dom.total_capacity = Bytes::from_mib(1024); dom.governed_capacity = Bytes::from_mib(1024);
  coord.register_domain(std::move(dom));

  const std::string cmd = args[0];
  try {
    if (cmd == "help") { return help(); }
    if (cmd == "list" && args.size() > 1 && args[1] == "models") {
      Json arr = Json::array();
      for (const auto& id : coord.models().model_ids()) { arr.push(id.to_hex()); }
      out(arr, json, "models: " + std::to_string(arr.array_size()));
    } else if (cmd == "list" && args.size() > 1 && args[1] == "residencies") {
      const Json st = coord.to_json();
      out(st, json, st.dump());
    } else if (cmd == "list" && args.size() > 1 && args[1] == "sets") {
      out(Json(static_cast<std::uint64_t>(coord.sets().size())), json,
          "residency sets: " + std::to_string(coord.sets().size()));
    } else if (cmd == "inspect" && args.size() > 1 && args[1] == "state") {
      const Json st = coord.to_json();
      out(st, json, st.dump());
    } else if (cmd == "inspect" && args.size() > 1 && args[1] == "capacity") {
      Json j = Json::object();
      for (const auto& did : coord.domains().ids()) {
        const MemoryDomain* d = coord.domains().find(did);
        if (!d) { continue; }
        Json dj = Json::object();
        dj.set("kind", memory_domain_kind_name(d->kind));
        dj.set("total", static_cast<std::uint64_t>(d->total_capacity.value()));
        dj.set("resident", static_cast<std::uint64_t>(d->resident().value()));
        dj.set("reserved", static_cast<std::uint64_t>(d->reserved().value()));
        dj.set("available", static_cast<std::uint64_t>(d->available().value()));
        j.set(did.to_hex(), std::move(dj));
      }
      out(j, json, j.dump());
    } else if (cmd == "load") {
      AdmitRequest req; req.model = model;
      LoadOutcome o = coord.load(req, AuthorityFrame{});
      Json j = Json::object();
      j.set("success", o.success); j.set("residency", o.residency.to_hex());
      j.set("bytes", static_cast<std::uint64_t>(o.loaded.value())); j.set("error", o.error);
      out(j, json, o.success ? "loaded residency " + o.residency.to_hex() : "load failed: " + o.error);
    } else if (cmd == "evict" && args.size() > 1) {
      EvictRequest ev; ev.residency = ResidencyId::from_hex(args[1]); ev.reason = EvictionReason::Manual;
      EvictionOutcome o = coord.evict(ev, AuthorityFrame{});
      out(Json(o.success), json, o.success ? "evicted" : "evict failed: " + o.error);
    } else if ((cmd == "pin" || cmd == "unpin") && args.size() > 1) {
      const ResidencyId id = ResidencyId::from_hex(args[1]);
      if (cmd == "pin") { coord.pin(id); } else { coord.unpin(id); }
      out(Json(true), json, std::string(cmd));
    } else if (cmd == "invalidate" && args.size() > 1) {
      coord.invalidate(ResidencyId::from_hex(args[1]), AuthorityFrame{});
      out(Json(true), json, "invalidated");
    } else if (cmd == "rebalance") {
      RebalancePlan plan = coord.rebalance(AuthorityFrame{});
      out(Json(static_cast<std::uint64_t>(plan.actions.size())), json, plan.summary);
    } else if (cmd == "roll") {
      ModelRevision nv = mrev; nv.artifact.generation = ArtifactGeneration(2);
      nv.revision = ModelRevisionId::random();
      coord.roll_model_generation(model, nv, AuthorityFrame{});
      out(Json(true), json, "model generation rolled");
    } else if (cmd == "snapshot") {
      const CoordinatorSnapshot snap = coord.snapshot();
      out(snap.to_json(), json, snap.to_json().dump());
    } else if (cmd == "save" && args.size() > 1) {
      coord.save_to(args[1]); out(Json(true), json, "saved to " + args[1]);
    } else if (cmd == "recover" && args.size() > 1) {
      coord.load_from(args[1]); out(Json(true), json, "recovered from " + args[1]);
    } else if (cmd == "run-demo") {
      AdmitRequest req; req.model = model;
      LoadOutcome o = coord.load(req, AuthorityFrame{});
      out(Json(o.success), json, o.success ? "synthetic demo loaded a residency" : o.error);
    } else if (cmd == "migrate" && args.size() > 2) {
      MigrateRequest mr_; mr_.residency = ResidencyId::from_hex(args[1]);
      mr_.dest_class = (args[2] == "pageable") ? ResidencyClass::PageableHost : ResidencyClass::PinnedHost;
      MigrateOutcome o = coord.migrate(mr_, AuthorityFrame{});
      out(Json(o.success), json, o.success ? "migrated" : "migrate failed: " + o.error);
    } else if (cmd == "demote" && args.size() > 1) {
      DemotionRequest dr_; dr_.residency = ResidencyId::from_hex(args[1]); dr_.dest_class = ResidencyClass::PageableHost;
      DemotionOutcome o = coord.demote(dr_, AuthorityFrame{});
      out(Json(o.success), json, o.success ? "demoted" : "demote failed: " + o.error);
    } else {
      return help();
    }
  } catch (const std::exception& e) {
    return fatal(e);
  }
  return 0;
}
