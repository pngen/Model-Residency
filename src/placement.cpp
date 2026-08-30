#include "mr/placement.hpp"

#include <cstdio>

namespace mr {

const PlacementCandidate* PlacementDecision::chosen() const {
  for (const auto& c : candidates) {
    if (c.final) return &c;
  }
  return candidates.empty() ? nullptr : &candidates.front();
}

Json PlacementDecision::to_json() const {
  Json root = Json::object();
  root.set("accepted", accepted);
  root.set("deferred", deferred);
  root.set("defer_reason", defer_reason);
  root.set("id", id.to_hex());
  root.set("tie_break", tie_break);
  root.set("summary", summary);

  Json cands = Json::array();
  for (const auto& c : candidates) {
    Json j = Json::object();
    j.set("node", c.node.to_hex());
    j.set("device", c.device.to_hex());
    j.set("domain", c.domain.to_hex());
    j.set("class", residency_class_name(c.cls));
    j.set("domain_kind", memory_domain_kind_name(c.domain_kind));
    j.set("hard_constraint_ok", c.hard_constraint_ok);
    j.set("estimated_movement", static_cast<std::uint64_t>(c.estimated_movement.value()));
    j.set("estimated_load_cost", c.estimated_load_cost);
    j.set("available_capacity", static_cast<std::uint64_t>(c.available_capacity.value()));
    j.set("locality", c.locality);
    j.set("existing_warm", c.existing_warm);
    j.set("existing_ready", c.existing_ready);
    j.set("eviction_required", c.eviction_required);
    j.set("migration_required", c.migration_required);
    j.set("expected_benefit", c.expected_benefit);
    j.set("ranking", c.ranking);
    j.set("chosen", c.chosen);
    j.set("final", c.final);
    Json reasons = Json::array();
    for (const auto& r : c.incompatible_reasons) { reasons.push(r); }
    j.set("incompatible_reasons", std::move(reasons));
    j.set("explanation", c.explanation.to_json());
    cands.push(std::move(j));
  }
  root.set("candidates", std::move(cands));

  Json factors = Json::array();
  for (const auto& f : deciding_factors) {
    Json jf = Json::object();
    jf.set("key", f.key);
    jf.set("value", f.value);
    jf.set("weight", f.weight);
    jf.set("kind", accounting_kind_name(f.kind));
    jf.set("note", f.note);
    factors.push(std::move(jf));
  }
  root.set("deciding_factors", std::move(factors));
  return root;
}

std::string PlacementDecision::to_text() const {
  std::string out;
  out += accepted ? "accepted: yes\n" : "accepted: no\n";
  if (deferred) out += "deferred: " + defer_reason + "\n";
  for (const auto& c : candidates) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "  rank %.3f %s%s%s%s  node=%s",
                  c.ranking,
                  residency_class_name(c.cls),
                  c.final ? "  [chosen]" : "",
                  c.existing_warm ? "  [warm]" : "",
                  c.hard_constraint_ok ? "" : "  [INCOMPATIBLE]",
                  c.node.to_hex().c_str());
    out += buf;
    out += '\n';
  }
  return out;
}

} // namespace mr
