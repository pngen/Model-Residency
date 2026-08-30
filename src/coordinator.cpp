#include "mr/coordinator.hpp"

#include <algorithm>
#include "mr/compatibility.hpp"
#include <cstdio>
#include <utility>

namespace mr {
namespace {

bool rank_better(const PlacementCandidate& a, const PlacementCandidate& b) {
  if (a.hard_constraint_ok != b.hard_constraint_ok) return a.hard_constraint_ok;
  if (a.ranking != b.ranking) return a.ranking > b.ranking;
  if (a.domain != b.domain) return a.domain < b.domain;
  if (a.device != b.device) return a.device < b.device;
  return a.node < b.node;
}

std::string why_incompat(const Compatibility& c) {
  std::string out;
  for (const auto& r : c.reasons()) {
    if (!out.empty()) out += "; ";
    out += r;
  }
  return out;
}

} // namespace

Coordinator::Coordinator(ResidencyBackend* backend, ResidencyPolicy policy)
    : backend_(backend), policy_(std::move(policy)) {}

CoordinatorEpoch Coordinator::epoch() const noexcept {
  std::lock_guard<std::mutex> l(mutex_);
  return epoch_;
}
PolicyGeneration Coordinator::policy_generation() const noexcept {
  std::lock_guard<std::mutex> l(mutex_);
  return policy_gen_;
}

void Coordinator::define_model(ModelRevision revision) {
  std::lock_guard<std::mutex> l(mutex_);
  models_.register_revision(std::move(revision));
  const ModelId m = models_.latest_revision(revision.model)->model;
  if (model_gens_.find(m) == model_gens_.end()) {
    model_gens_[m] = ModelGeneration::first();
  }
}

void Coordinator::register_adapter_set(AdapterSet set) {
  std::lock_guard<std::mutex> l(mutex_);
  throw_unless(set.is_valid(), ErrorCode::InvalidIdentity, "invalid adapter set");
  // Ensure the base model is known.
  throw_unless(models_.has(set.base_model), ErrorCode::InvalidIdentity, "adapter set base model unknown");
  for (const auto& a : set.adapters) {
    throw_unless(a.is_valid(), ErrorCode::InvalidIdentity, "invalid adapter in set");
  }
  for (auto& existing : adapter_sets_) {
    if (existing.id == set.id) {
      throw_error(ErrorCode::AlreadyExists, "adapter set already registered");
    }
  }
  adapter_sets_.push_back(std::move(set));
}

void Coordinator::register_domain(MemoryDomain domain) {
  std::lock_guard<std::mutex> l(mutex_);
  domains_.register_domain(std::move(domain));
}

void Coordinator::register_worker(WorkerRegistration reg) {
  std::lock_guard<std::mutex> l(mutex_);
  throw_unless(!reg.worker.is_nil() && !reg.boot.is_nil(), ErrorCode::InvalidIdentity, "invalid worker registration");
  workers_[reg.worker] = reg;
  // Register device generations.
  for (std::size_t i = 0; i < reg.devices.size(); ++i) {
    if (i < reg.device_generations.size()) {
      device_gens_[reg.devices[i]] = reg.device_generations[i];
    }
  }
}

void Coordinator::mark_worker_lost(WorkerId worker, const AuthorityFrame& a) {
  (void)a;
  std::lock_guard<std::mutex> l(mutex_);
  auto it = workers_.find(worker);
  if (it == workers_.end()) return;
  const WorkerRegistration reg = it->second;
  // Invalidate live residency on the lost worker's node/device set. No live
  // device residency survives process loss; we surrender the capacity.
  for (auto& [rid, res] : residencies_) {
    if (res.node == reg.node || std::find(reg.devices.begin(), reg.devices.end(), res.device) != reg.devices.end()) {
      if (res.is_resident_state() || res.lifecycle == LifecycleState::Allocating ||
          res.lifecycle == LifecycleState::Loading || res.lifecycle == LifecycleState::Validating) {
        if (auto* d = domains_.find(res.domain)) {
          d->release(res.allocated_bytes.value() > 0 ? res.allocated_bytes : res.byte_size);
        }
        res.authority = AuthorityState::Stale;
        res.readiness = ReadinessState::Failed;
        try { res.transition_to(LifecycleState::Invalidated); } catch (const Error&) {}
      }
    }
  }
  // Mark device domains unavailable if owned by this worker.
  for (const auto& dev : reg.devices) {
    if (auto* d = domains_.find_device(dev, MemoryDomainKind::Accelerator)) {
      d->set_unavailable(d->total_capacity);
    }
  }
}
// ---------------------------------------------------------------------------
// Small domain helpers
// ---------------------------------------------------------------------------
namespace {

MemoryDomainKind kind_for_class(ResidencyClass c) noexcept {
  switch (c) {
    case ResidencyClass::AcceleratorLocal: return MemoryDomainKind::Accelerator;
    case ResidencyClass::PinnedHost: return MemoryDomainKind::PinnedHost;
    case ResidencyClass::PageableHost: return MemoryDomainKind::PageableHost;
    case ResidencyClass::PersistentStorageRef: return MemoryDomainKind::PersistentStorage;
    case ResidencyClass::ProcessLocal: return MemoryDomainKind::ProcessLocal;
    case ResidencyClass::SharedHostMapping: return MemoryDomainKind::SharedHost;
    case ResidencyClass::GenericBackendMemory: return MemoryDomainKind::GenericBackend;
  }
  return MemoryDomainKind::PageableHost;
}

ResidencyClass class_for_kind(MemoryDomainKind k) noexcept {
  switch (k) {
    case MemoryDomainKind::Accelerator: return ResidencyClass::AcceleratorLocal;
    case MemoryDomainKind::PinnedHost: return ResidencyClass::PinnedHost;
    case MemoryDomainKind::PageableHost: return ResidencyClass::PageableHost;
    case MemoryDomainKind::PersistentStorage: return ResidencyClass::PersistentStorageRef;
    case MemoryDomainKind::ProcessLocal: return ResidencyClass::ProcessLocal;
    case MemoryDomainKind::SharedHost: return ResidencyClass::SharedHostMapping;
    case MemoryDomainKind::GenericBackend: return ResidencyClass::GenericBackendMemory;
  }
  return ResidencyClass::PageableHost;
}

struct LoadComponent {
  ShardDescriptor* shard = nullptr;
  const Adapter* adapter = nullptr;
  Bytes bytes{0};
  bool required = true;
  std::string label;
};

std::vector<LoadComponent> build_components(ModelRevision& mrev, const AdapterSet* aset) {
  std::vector<LoadComponent> comps;
  if (mrev.shards.empty()) {
    LoadComponent c;
    c.bytes = mrev.required_memory;
    c.required = true;
    c.label = "model";
    comps.push_back(c);
  } else {
    for (auto& s : mrev.shards) {
      LoadComponent c;
      c.shard = &s;
      c.bytes = s.size;
      c.required = s.required;
      c.label = "shard-" + std::to_string(s.index);
      comps.push_back(c);
    }
  }
  if (aset != nullptr) {
    for (const auto& a : aset->adapters) {
      LoadComponent c;
      c.adapter = &a;
      c.bytes = a.size;
      c.required = true;
      c.label = "adapter-" + a.revision;
      comps.push_back(c);
    }
  }
  return comps;
}

} // namespace

AdmissionResult Coordinator::admit(const AdmitRequest& req, const AuthorityFrame&) {
  std::lock_guard<std::mutex> l(mutex_);
  AdmissionResult out;

  const ModelRevision* pkg = req.model_revision.is_nil()
                                 ? models_.latest_revision(req.model)
                                 : models_.find(req.model, req.model_revision);
  if (pkg == nullptr) {
    out.verdict = AdmissionVerdict::Reject;
    out.reason = "unknown model or revision";
    return out;
  }
  ModelRevision mrev = *pkg;

  const AdapterSet* aset_ptr = nullptr;
  if (!req.adapter_set.is_nil()) {
    for (const auto& s : adapter_sets_) {
      if (s.id == req.adapter_set) { aset_ptr = &s; break; }
    }
    if (aset_ptr == nullptr) {
      out.verdict = AdmissionVerdict::Reject;
      out.reason = "unknown adapter set";
      return out;
    }
    if (aset_ptr->base_model != mrev.model) {
      out.verdict = AdmissionVerdict::Reject;
      out.reason = "adapter set base model mismatch";
      return out;
    }
  }

  const ModelGeneration model_gen = model_gens_.count(mrev.model)
                                        ? model_gens_[mrev.model]
                                        : ModelGeneration::first();

  for (const auto& [rid, res] : residencies_) {
    if (res.model == mrev.model && res.adapter_set == req.adapter_set &&
        res.model_generation == model_gen && res.lifecycle == LifecycleState::Ready &&
        res.authority == AuthorityState::Authoritative) {
      out.verdict = AdmissionVerdict::Accept;
      out.target_residency = rid;
      out.reason = "reuse-existing-ready";
      return out;
    }
  }

  CompatibilityChecker checker;
  std::vector<PlacementCandidate> candidates;
  Bytes required = mrev.required_memory;
  if (aset_ptr != nullptr) { required = Bytes(required.value() + aset_ptr->aggregate_size.value()); }

  for (const auto& did : domains_.ids()) {
    const MemoryDomain* d = domains_.find(did);
    PlacementCandidate c;
    c.domain = did;
    c.device = d->device;
    c.node = d->node;
    c.cls = class_for_kind(d->kind);
    c.domain_kind = d->kind;
    c.available_capacity = d->available();

    auto cb = checker.check_model_backend(mrev, d->backend);
    auto cd = checker.check_model_device(mrev, d->capability);
    if (cb.incompatible() || cd.incompatible()) {
      c.hard_constraint_ok = false;
      if (cb.incompatible()) { c.incompatible_reasons.push_back(why_incompat(cb)); }
      if (cd.incompatible()) { c.incompatible_reasons.push_back(why_incompat(cd)); }
    } else {
      c.hard_constraint_ok = true;
    }
    c.estimated_movement = required;
    c.locality = (!d->device.is_nil()) ? 1.0 : 0.4;
    for (const auto& [rid, res] : residencies_) {
      if (res.domain == did && res.model == mrev.model && res.is_resident_state()) {
        c.existing_warm = true;
        c.warm_residency = rid;
        c.existing_ready = res.lifecycle == LifecycleState::Ready;
        if (res.lifecycle == LifecycleState::Ready) { c.estimated_movement = Bytes(0); }
        break;
      }
    }

    double score = 0.0;
    if (!c.hard_constraint_ok) {
      score = -1e9;
    } else {
      score += c.existing_ready ? 100.0 : 0.0;
      score += c.existing_warm ? 50.0 : 0.0;
      score += 10.0 * c.locality;
      score += (c.available_capacity.value() >= required.value()) ? 5.0 : -20.0;
      score += static_cast<double>(req.priority);
      score += req.expected_demand;
    }
    c.ranking = score;
    candidates.push_back(std::move(c));
  }
  std::sort(candidates.begin(), candidates.end(), rank_better);
  out.placement.candidates = candidates;

  PlacementCandidate* best = nullptr;
  for (auto& c : candidates) {
    if (c.hard_constraint_ok) { best = &c; break; }
  }

  if (best == nullptr) {
    out.verdict = AdmissionVerdict::Reject;
    out.reason = "no compatible candidate location";
    return out;
  }

  if (best->available_capacity.value() < required.value()) {
    if (req.allow_eviction) {
      out.verdict = AdmissionVerdict::RequireEviction;
      out.reason = "target domain has insufficient capacity; eviction required";
    } else {
      out.verdict = AdmissionVerdict::Reject;
      out.reason = "capacity exceeded and eviction not allowed";
    }
    return out;
  }

  out.verdict = AdmissionVerdict::Accept;
  out.reason = "accepted";
  best->final = true;
  best->chosen = true;
  return out;
}
// ---------------------------------------------------------------------------
// Authority gating helper
// ---------------------------------------------------------------------------
namespace {
bool authority_accepts(const CoordinatorEpoch& caller, const CoordinatorEpoch& current,
                       const ModelGeneration& caller_model_gen, const ModelGeneration& current_model_gen) {
  if (!caller.is_zero() && caller != current) return false;
  if (!caller_model_gen.is_zero() && caller_model_gen != current_model_gen) return false;
  return true;
}
} // namespace

// ---------------------------------------------------------------------------
// Load (admission + transactional loading)
// ---------------------------------------------------------------------------
LoadOutcome Coordinator::load(const AdmitRequest& req, const AuthorityFrame& a) {
  LoadOutcome outcome;

  // Warm-reuse fast path.
  AdmissionResult ad = admit(req, a);
  if (ad.verdict == AdmissionVerdict::Accept && !ad.target_residency.is_nil()) {
    std::lock_guard<std::mutex> l(mutex_);
    if (auto* r = residencies_.find(ad.target_residency) != residencies_.end() ? &residencies_[ad.target_residency] : nullptr) {
      r->touch();
      outcome.success = true;
      outcome.residency = ad.target_residency;
      outcome.loaded = r->byte_size;
      return outcome;
    }
  }

  // Eviction-required path: evict safe candidates then re-admit.
  int evictions = 0;
  while (ad.verdict == AdmissionVerdict::RequireEviction && evictions < 8) {
    EvictRequest ev;
    // Deterministically pick the first reclaimable, unprotected residency on the
    // candidate domain that is not the only authoritative copy of its model.
    std::lock_guard<std::mutex> l(mutex_);
    ResidencyId victim;
    if (!ad.placement.candidates.empty()) {
      const MemoryDomainId dom = ad.placement.candidates.front().domain;
      for (const auto& [rid, res] : residencies_) {
        if (res.domain != dom) continue;
        if (!res.is_resident_state()) continue;
        if (res.active_references > 0 || res.pinned) continue;
        if (res.lifecycle == LifecycleState::Migrating || res.lifecycle == LifecycleState::Evicting) continue;
        victim = rid;
        break;
      }
    }
    if (victim.is_nil()) break;
    ev.residency = victim;
    ev.reason = EvictionReason::CapacityPressure;
    ev.force = false;
    EvictionOutcome eo = evict(ev, a);
    if (!eo.success) break;
    ++evictions;
    ad = admit(req, a);
  }

  if (ad.verdict != AdmissionVerdict::Accept) {
    outcome.success = false;
    outcome.error = ad.reason;
    return outcome;
  }

  const PlacementCandidate* chosen = ad.placement.chosen();
  if (chosen == nullptr) {
    outcome.success = false;
    outcome.error = "placement chose no candidate";
    return outcome;
  }

  // Resolve the model revision and adapter set up front.
  std::unique_lock<std::mutex> l(mutex_);
  const ModelRevision* model_ptr = req.model_revision.is_nil()
                                       ? models_.latest_revision(req.model)
                                       : models_.find(req.model, req.model_revision);
  if (model_ptr == nullptr) {
    outcome.success = false;
    outcome.error = "unknown model revision";
    return outcome;
  }
  ModelRevision mrev = *model_ptr;
  const AdapterSet* aset_ptr = nullptr;
  AdapterSet aset_copy;
  if (!req.adapter_set.is_nil()) {
    for (const auto& s : adapter_sets_) {
      if (s.id == req.adapter_set) { aset_ptr = &s; break; }
    }
    if (aset_ptr == nullptr) {
      outcome.success = false;
      outcome.error = "unknown adapter set";
      return outcome;
    }
    aset_copy = *aset_ptr;
    aset_ptr = &aset_copy;
  }
  const ModelGeneration model_gen = model_gens_.count(mrev.model) ? model_gens_[mrev.model] : ModelGeneration::first();

  MemoryDomain* domain = domains_.find(chosen->domain);
  if (domain == nullptr || !domain->can_reserve(mrev.required_memory)) {
    outcome.success = false;
    outcome.error = "capacity unavailable at load time";
    return outcome;
  }
  (void)domain;

  // Build the set of residency members.
  std::vector<ResidencyId> member_ids;
  ResidencySet set;
  set.id = ResidencySetId::random();
  set.model = mrev.model;
  set.model_revision = mrev.revision;
  set.adapter_set = req.adapter_set;
  set.generation = next_residency_gen_locked();

  for (const auto& comp : build_components(mrev, aset_ptr)) {
    Residency res;
    res.id = ResidencyId::random();
    res.model = mrev.model;
    res.model_revision = mrev.revision;
    res.artifact = mrev.artifact.id;
    res.artifact_generation = mrev.artifact.generation;
    res.adapter_set = req.adapter_set;
    res.node = chosen->node;
    res.device = chosen->device;
    res.domain = chosen->domain;
    res.backend = domain->backend;
    res.capability = domain->capability;
    res.cls = chosen->cls;
    res.domain_kind = chosen->domain_kind;
    res.byte_size = comp.bytes;
    res.allocated_bytes = comp.bytes;
    res.loaded_bytes = Bytes(0);
    res.validation = ValidationState::Unvalidated;
    res.readiness = ReadinessState::NotReady;
    res.compatibility = CompatibilityState::Compatible;
    res.lifecycle = LifecycleState::Declared;
    res.authority = AuthorityState::Provisional;
    res.generation = next_residency_gen_locked();
    res.source_provenance = comp.label;
    res.model_generation = model_gen;
    res.adapter_generation = aset_ptr ? aset_ptr->generation : AdapterGeneration::zero();
    res.device_generation = device_gens_.count(chosen->device) ? device_gens_[chosen->device] : DeviceGeneration::zero();
    res.policy_generation = policy_gen_;

    member_ids.push_back(res.id);
    set.members.push_back(res.id);
    set.required_members.push_back(res.id);
    residencies_[res.id] = std::move(res);
  }
  set.aggregate_bytes = Bytes(mrev.required_memory.value() + (aset_ptr ? aset_ptr->aggregate_size.value() : 0));
  set.complete = false;
  set.ready = false;
  sets_[set.id] = set;
  active_set_by_model_[mrev.model] = set.id;

  // Reserve aggregate capacity on the chosen domain.
  const Bytes aggregate_bytes = set.aggregate_bytes;
  if (!domain->reserve(aggregate_bytes)) {
    outcome.success = false;
    outcome.error = "could not reserve capacity";
    return outcome;
  }

  const CoordinatorEpoch epoch_before = epoch_;
  const ModelGeneration model_gen_before = model_gen;
  (void)a;
  std::vector<LoadTarget> targets;
  targets.reserve(member_ids.size());
  for (std::size_t i = 0; i < member_ids.size(); ++i) {
    LoadTarget t;
    t.domain = chosen->domain;
    t.kind = chosen->domain_kind;
    t.cls = chosen->cls;
    t.device = chosen->device;
    t.node = chosen->node;
    t.backend = domain->backend;
    targets.push_back(t);
  }

  // Byte movement happens OUTSIDE the master lock (never call the backend while
  // holding it).
  l.unlock();
  std::vector<LoadResult> results;
  results.reserve(targets.size());
  for (const auto& t : targets) {
    results.push_back(backend_ ? backend_->load(t, mrev, aset_ptr) : LoadResult{});
  }
  l.lock();

  // Re-acquire and verify nothing stale happened while we were unlocked.
  if (epoch_ != epoch_before || model_gens_[mrev.model] != model_gen_before) {
    if (auto* d = domains_.find(chosen->domain)) { d->cancel_reservation(aggregate_bytes); }
    for (const auto& mid : member_ids) {
      auto& res = residencies_[mid];
      res.authority = AuthorityState::Stale;
      res.readiness = ReadinessState::Failed;
      try { res.transition_to(LifecycleState::Failed); } catch (const Error&) {}
    }
    outcome.success = false;
    outcome.error = "stale authority detected during load";
    return outcome;
  }
  domain = domains_.find(chosen->domain);

  for (std::size_t i = 0; i < member_ids.size(); ++i) {
    auto& res = residencies_[member_ids[i]];
    if (results[i].success && res.lifecycle == LifecycleState::Declared && domain != nullptr) {
      res.validation = ValidationState::Valid;
      res.loaded_bytes = results[i].loaded;
      res.handle = results[i].handle;
      res.load_time_ns = monotonic_ns();
      res.last_use_ns = res.load_time_ns;
      res.touch();
      res.transition_to(LifecycleState::Planned);
      res.transition_to(LifecycleState::Allocating);
      res.transition_to(LifecycleState::Loading);
      res.transition_to(LifecycleState::Validating);
      res.transition_to(LifecycleState::Resident);
      res.readiness = ReadinessState::Loading;
      res.authority = AuthorityState::Provisional;
      domain->commit(res.allocated_bytes);
    } else {
      res.authority = AuthorityState::Stale;
      res.readiness = ReadinessState::Failed;
      try { res.transition_to(LifecycleState::Failed); } catch (const Error&) {}
    }
  }

  bool all_ok = true;
  for (const auto& mid : member_ids) {
    if (residencies_[mid].lifecycle != LifecycleState::Resident) { all_ok = false; break; }
  }
  if (all_ok) {
    for (const auto& mid : member_ids) {
      auto& res = residencies_[mid];
      res.transition_to(LifecycleState::Ready);
      res.readiness = ReadinessState::Ready;
      res.authority = AuthorityState::Authoritative;
    }
    auto& s = sets_[set.id];
    s.complete = true;
    s.ready = true;
    outcome.success = true;
    outcome.residency = member_ids.front();
    outcome.set = set.id;
    outcome.loaded = set.aggregate_bytes;
  } else {
    auto& s = sets_[set.id];
    s.complete = s.recompute_complete();
    s.ready = false;
    s.failure_reason = "partial or failed load";
    outcome.success = false;
    outcome.error = "load did not complete all required members";
  }
  return outcome;
}
// ---------------------------------------------------------------------------
// Publish ready
// ---------------------------------------------------------------------------
void Coordinator::publish_ready(ResidencyId id, const AuthorityFrame& a) {
  std::lock_guard<std::mutex> l(mutex_);
  throw_unless(a.coordinator_epoch.is_zero() || a.coordinator_epoch == epoch_,
               ErrorCode::StaleAuthority, "stale coordinator epoch on publish");
  auto it = residencies_.find(id);
  throw_unless(it != residencies_.end(), ErrorCode::NotFound, "unknown residency");
  Residency& res = it->second;
  if (res.lifecycle == LifecycleState::Ready && res.authority == AuthorityState::Authoritative) return;
  throw_unless(res.lifecycle == LifecycleState::Resident, ErrorCode::InvalidTransition,
               "residency must be Resident before publish_ready");
  // Group completeness check.
  for (auto& [sid, set] : sets_) {
    if (std::find(set.members.begin(), set.members.end(), id) != set.members.end()) {
      throw_unless(set.recompute_complete(), ErrorCode::InvalidState, "set is not complete");
    }
  }
  res.transition_to(LifecycleState::Ready);
  res.readiness = ReadinessState::Ready;
  res.authority = AuthorityState::Authoritative;
}

// ---------------------------------------------------------------------------
// Migrate
// ---------------------------------------------------------------------------
MigrateOutcome Coordinator::migrate(const MigrateRequest& req, const AuthorityFrame& a) {
  MigrateOutcome out;
  std::unique_lock<std::mutex> l(mutex_);
  throw_unless(a.coordinator_epoch.is_zero() || a.coordinator_epoch == epoch_,
               ErrorCode::StaleAuthority, "stale coordinator epoch on migrate");
  auto it = residencies_.find(req.residency);
  if (it == residencies_.end()) { out.error = "unknown residency"; return out; }
  Residency& src = it->second;
  if (!src.is_resident_state()) { out.error = "source not resident"; return out; }
  if (src.active_references > 0) { out.error = "source has active references"; return out; }
  if (src.lifecycle == LifecycleState::Migrating) { out.error = "already migrating"; return out; }

  const MemoryDomainKind dest_kind = kind_for_class(req.dest_class);
  MemoryDomain* dest = nullptr;
  for (const auto& did : domains_.ids()) {
    MemoryDomain* d = domains_.find(did);
    if (d->kind != dest_kind) continue;
    if (!req.dest_device.is_nil() && d->device != req.dest_device) continue;
    if (dest == nullptr || d->available().value() > dest->available().value()) { dest = d; }
  }
  if (dest == nullptr) { out.error = "no destination domain"; return out; }
  const Bytes bytes = src.byte_size;
  if (!dest->can_reserve(bytes)) { out.error = "destination capacity insufficient"; return out; }
  dest->reserve(bytes);

  Residency dst;
  dst.id = ResidencyId::random();
  dst.model = src.model;
  dst.model_revision = src.model_revision;
  dst.artifact = src.artifact;
  dst.artifact_generation = src.artifact_generation;
  dst.adapter_set = src.adapter_set;
  dst.node = req.dest_node.is_nil() ? src.node : req.dest_node;
  dst.device = dest->device;
  dst.domain = dest->id;
  dst.backend = dest->backend;
  dst.capability = dest->capability;
  dst.cls = req.dest_class;
  dst.domain_kind = dest_kind;
  dst.byte_size = bytes;
  dst.allocated_bytes = bytes;
  dst.loaded_bytes = Bytes(0);
  dst.validation = ValidationState::Unvalidated;
  dst.readiness = ReadinessState::NotReady;
  dst.compatibility = src.compatibility;
  dst.lifecycle = LifecycleState::Declared;
  dst.authority = AuthorityState::Provisional;
  dst.generation = next_residency_gen_locked();
  dst.model_generation = src.model_generation;
  dst.adapter_generation = src.adapter_generation;
  dst.device_generation = device_gens_.count(dest->device) ? device_gens_[dest->device] : DeviceGeneration::zero();
  dst.policy_generation = policy_gen_;
  dst.source_provenance = "migrated from " + src.id.to_hex();

  const MigrationId mid = MigrationId::random();
  src.migration = mid;
  dst.migration = mid;
  src.transition_to(LifecycleState::Migrating);
  dst.transition_to(LifecycleState::Allocating);

  // Capture data and move bytes outside the lock.
  ModelRevision mrev{};
  if (const ModelRevision* mp = models_.find(src.model, src.model_revision)) { mrev = *mp; }
  AdapterSet aset_copy;
  const AdapterSet* aset_ptr = nullptr;
  if (!src.adapter_set.is_nil()) {
    for (const auto& s : adapter_sets_) {
      if (s.id == src.adapter_set) { aset_copy = s; break; }
    }
    aset_ptr = &aset_copy;
  }
  LoadTarget fromT;
  fromT.domain = src.domain; fromT.kind = src.domain_kind; fromT.cls = src.cls;
  fromT.device = src.device; fromT.node = src.node; fromT.backend = src.backend;
  LoadTarget toT;
  toT.domain = dest->id; toT.kind = dest_kind; toT.cls = req.dest_class;
  toT.device = dest->device; toT.node = dst.node; toT.backend = dest->backend;
  const ResidencyId src_id = src.id;
  const MemoryDomainId src_dom = src.domain;
  const Bytes src_bytes = src.allocated_bytes;
  void* src_handle = src.handle;
  const CoordinatorEpoch ep_before = epoch_;
  const ModelGeneration mgen = src.model_generation;
  const ModelId model = src.model;

  l.unlock();
  LoadResult r = backend_ ? backend_->migrate(fromT, toT, mrev, aset_ptr, src_handle, bytes) : LoadResult{};
  l.lock();

  double rank = 0;
  (void)rank;
  if (epoch_ != ep_before || model_gens_[model] != mgen) {
    // Revert: source stays authoritative.
    if (auto* d = domains_.find(dest->id)) { d->cancel_reservation(bytes); }
    try { src.transition_to(LifecycleState::Resident); } catch (const Error&) {}
    out.error = "stale authority detected during migration";
    return out;
  }
  MemoryDomain* dest_now = domains_.find(dest->id);
  if (!r.success || dest_now == nullptr) {
    if (dest_now) { dest_now->cancel_reservation(bytes); }
    try { src.transition_to(LifecycleState::Resident); } catch (const Error&) {}
    out.error = r.success ? "destination domain lost" : "(migrate failed) " + r.error;
    return out;
  }
  // Success: validate + publish destination.
  dst.validation = ValidationState::Valid;
  dst.loaded_bytes = r.loaded;
  dst.handle = r.handle;
  dst.load_time_ns = monotonic_ns();
  dst.last_use_ns = dst.load_time_ns;
  dst.transition_to(LifecycleState::Validating);
  dst.transition_to(LifecycleState::Resident);
  dest_now->commit(bytes);
  if (residency_class_is_execution_ready(dst.cls)) {
    dst.transition_to(LifecycleState::Ready);
    dst.readiness = ReadinessState::Ready;
    dst.authority = AuthorityState::Authoritative;
  } else {
    dst.readiness = ReadinessState::NotReady;
    dst.authority = AuthorityState::Provisional;
  }
  residencies_[dst.id] = dst;
  // Retire the source and release its capacity.
  if (auto* d = domains_.find(src_dom)) { d->release(src_bytes); }
  src.authority = AuthorityState::Stale;
  src.readiness = ReadinessState::Stale;
  try { src.transition_to(LifecycleState::Retired); } catch (const Error&) {}
  // Update set membership: replace source with destination.
  ResidencySetId owning_set = ResidencySetId::nil();
  for (auto& [sid, set] : sets_) {
    auto m = std::find(set.members.begin(), set.members.end(), src_id);
    if (m != set.members.end()) {
      *m = dst.id;
      owning_set = sid;
    }
    auto rm = std::find(set.required_members.begin(), set.required_members.end(), src_id);
    if (rm != set.required_members.end()) { *rm = dst.id; }
  }
  if (!owning_set.is_nil()) { active_set_by_model_[model] = owning_set; }
  out.success = true;
  out.migration = mid;
  out.dest_residency = dst.id;
  out.bytes = bytes;
  return out;
}

// ---------------------------------------------------------------------------
// Evict
// ---------------------------------------------------------------------------
EvictionOutcome Coordinator::evict(const EvictRequest& req, const AuthorityFrame& a) {
  EvictionOutcome out;
  std::lock_guard<std::mutex> l(mutex_);
  throw_unless(a.coordinator_epoch.is_zero() || a.coordinator_epoch == epoch_,
               ErrorCode::StaleAuthority, "stale coordinator epoch on evict");
  auto it = residencies_.find(req.residency);
  if (it == residencies_.end()) { out.error = "unknown residency"; return out; }
  Residency& res = it->second;
  if (!res.is_resident_state()) { out.error = "residency not resident"; return out; }
  if (res.active_references > 0) { out.error = "residency has active references"; return out; }
  if (res.pinned && !(req.force && policy_.allow_force_eviction)) {
    out.error = "residency is pinned or protected";
    return out;
  }
  if (res.lifecycle == LifecycleState::Migrating) { out.error = "migration in progress"; return out; }
  // Only-authoritative protection.
  if (!policy_.allow_evict_last_authoritative) {
    bool other = false;
    for (const auto& [rid, r2] : residencies_) {
      if (r2.model == res.model && r2.id != res.id && r2.lifecycle == LifecycleState::Ready &&
          r2.authority == AuthorityState::Authoritative) {
        other = true; break;
      }
    }
    if (!other) { out.error = "cannot evict the only authoritative copy"; return out; }
  }

  res.eviction = EvictionId::random();
  res.eviction_reason = req.reason;
  res.transition_to(LifecycleState::Evicting);
  res.transition_to(LifecycleState::Evicted);
  if (auto* d = domains_.find(res.domain)) { d->release(res.allocated_bytes); }
  res.authority = AuthorityState::Stale;
  res.readiness = ReadinessState::Stale;
  if (backend_ && res.handle) { backend_->evict(res.cls, res.handle); }
  res.handle = nullptr;
  // Update set membership + readiness.
  for (auto& [sid, set] : sets_) {
    auto m = std::find(set.members.begin(), set.members.end(), res.id);
    if (m != set.members.end()) { set.members.erase(m); }
    auto rm = std::find(set.required_members.begin(), set.required_members.end(), res.id);
    if (rm != set.required_members.end()) { set.required_members.erase(rm); }
    set.complete = set.recompute_complete();
    set.ready = false;
  }
  out.success = true;
  out.eviction = res.eviction;
  return out;
}

// ---------------------------------------------------------------------------
// Demote
// ---------------------------------------------------------------------------
DemotionOutcome Coordinator::demote(const DemotionRequest& req, const AuthorityFrame& a) {
  DemotionOutcome out;
  std::unique_lock<std::mutex> l(mutex_);
  throw_unless(a.coordinator_epoch.is_zero() || a.coordinator_epoch == epoch_,
               ErrorCode::StaleAuthority, "stale coordinator epoch on demote");
  auto it = residencies_.find(req.residency);
  if (it == residencies_.end()) { out.error = "unknown residency"; return out; }
  Residency& res = it->second;
  if (!res.is_resident_state()) { out.error = "residency not resident"; return out; }
  if (res.active_references > 0) { out.error = "residency has active references"; return out; }
  const MemoryDomainKind dest_kind = kind_for_class(req.dest_class);
  MemoryDomain* dest = nullptr;
  for (const auto& did : domains_.ids()) {
    MemoryDomain* d = domains_.find(did);
    if (d->kind != dest_kind) continue;
    if (dest == nullptr || d->available().value() > dest->available().value()) { dest = d; }
  }
  if (dest == nullptr) { out.error = "no demotion destination domain"; return out; }
  const Bytes bytes = res.byte_size;
  if (!dest->can_reserve(bytes)) { out.error = "demotion destination capacity insufficient"; return out; }
  dest->reserve(bytes);
  const ResidencyId rid = res.id;
  const MemoryDomainId src_dom = res.domain;
  const Bytes src_bytes = res.allocated_bytes;
  void* src_handle = res.handle;
  res.transition_to(LifecycleState::Demoting);

  ModelRevision mrev{};
  if (const ModelRevision* mp = models_.find(res.model, res.model_revision)) { mrev = *mp; }
  LoadTarget fromT; fromT.domain = res.domain; fromT.kind = res.domain_kind; fromT.cls = res.cls;
  fromT.device = res.device; fromT.node = res.node; fromT.backend = res.backend;
  LoadTarget toT; toT.domain = dest->id; toT.kind = dest_kind; toT.cls = req.dest_class;
  toT.device = dest->device; toT.node = dest->node; toT.backend = dest->backend;
  const MemoryDomainId dest_dom = dest->id;
  const CoordinatorEpoch ep_before = epoch_;
  l.unlock();
  LoadResult r = backend_ ? backend_->migrate(fromT, toT, mrev, nullptr, src_handle, bytes) : LoadResult{};
  l.lock();
  if (epoch_ != ep_before) {
    if (auto* d = domains_.find(dest_dom)) { d->cancel_reservation(bytes); }
    try { res.transition_to(LifecycleState::Resident); } catch (const Error&) {}
    out.error = "stale authority during demote";
    return out;
  }
  MemoryDomain* dest_now = domains_.find(dest_dom);
  if (!r.success || dest_now == nullptr) {
    if (dest_now) { dest_now->cancel_reservation(bytes); }
    try { res.transition_to(LifecycleState::Resident); } catch (const Error&) {}
    out.error = "demote transfer failed";
    return out;
  }
  // Commit: change the residency's class/domain; keep identity + generation.
  if (auto* d = domains_.find(src_dom)) { d->release(src_bytes); }
  dest_now->commit(bytes);
  res.domain = dest_dom;
  res.cls = req.dest_class;
  res.domain_kind = dest_kind;
  res.device = dest_now->device;
  res.node = dest_now->node;
  res.handle = r.handle;
  res.validation = ValidationState::Valid;
  res.transition_to(LifecycleState::Resident);
  res.readiness = residency_class_is_execution_ready(res.cls) ? ReadinessState::Ready : ReadinessState::NotReady;
  res.authority = residency_class_is_execution_ready(res.cls) ? AuthorityState::Authoritative : AuthorityState::Provisional;
  out.success = true;
  return out;
}

// ---------------------------------------------------------------------------
// Pin / unpin / invalidate
// ---------------------------------------------------------------------------
void Coordinator::pin(ResidencyId id) {
  std::lock_guard<std::mutex> l(mutex_);
  auto it = residencies_.find(id);
  throw_unless(it != residencies_.end(), ErrorCode::NotFound, "unknown residency");
  Residency& res = it->second;
  if (res.pinned) return;
  MemoryDomain* d = domains_.find(res.domain);
  throw_unless(d != nullptr, ErrorCode::NotFound, "unknown residency domain");
  throw_unless(d->pin(res.allocated_bytes), ErrorCode::CapacityExceeded, "cannot pin: exceeds reclaimable");
  res.pinned = true;
}

void Coordinator::unpin(ResidencyId id) {
  std::lock_guard<std::mutex> l(mutex_);
  auto it = residencies_.find(id);
  throw_unless(it != residencies_.end(), ErrorCode::NotFound, "unknown residency");
  Residency& res = it->second;
  if (!res.pinned) return;
  MemoryDomain* d = domains_.find(res.domain);
  throw_unless(d != nullptr, ErrorCode::NotFound, "unknown residency domain");
  d->unpin(res.allocated_bytes);
  res.pinned = false;
}

void Coordinator::invalidate(ResidencyId id, const AuthorityFrame& a) {
  std::lock_guard<std::mutex> l(mutex_);
  throw_unless(a.coordinator_epoch.is_zero() || a.coordinator_epoch == epoch_,
               ErrorCode::StaleAuthority, "stale coordinator epoch on invalidate");
  auto it = residencies_.find(id);
  throw_unless(it != residencies_.end(), ErrorCode::NotFound, "unknown residency");
  Residency& res = it->second;
  if (!res.is_resident_state()) { throw_error(ErrorCode::InvalidTransition, "residency not resident"); }
  if (auto* d = domains_.find(res.domain)) { d->release(res.allocated_bytes); }
  if (backend_ && res.handle) { backend_->evict(res.cls, res.handle); res.handle = nullptr; }
  res.authority = AuthorityState::Stale;
  res.readiness = ReadinessState::Stale;
  res.transition_to(LifecycleState::Invalidated);
  for (auto& [sid, set] : sets_) {
    auto m = std::find(set.members.begin(), set.members.end(), id);
    if (m != set.members.end()) { set.members.erase(m); }
    auto rm = std::find(set.required_members.begin(), set.required_members.end(), id);
    if (rm != set.required_members.end()) { set.required_members.erase(rm); }
    set.complete = set.recompute_complete();
    set.ready = false;
  }
}

// ---------------------------------------------------------------------------
// Generation rollover
// ---------------------------------------------------------------------------
void Coordinator::roll_model_generation(ModelId model, ModelRevision new_revision, const AuthorityFrame& a) {
  std::lock_guard<std::mutex> l(mutex_);
  throw_unless(a.coordinator_epoch.is_zero() || a.coordinator_epoch == epoch_,
               ErrorCode::StaleAuthority, "stale coordinator epoch on rollover");
  models_.register_revision(std::move(new_revision));
  ModelGeneration& gen = model_gens_[model];
  gen = gen.next();
  // Invalidate every residency of the old generation; old generation remains
  // distinguishable (its generation value is retained).
  const ModelGeneration new_gen = gen;
  for (auto& [rid, res] : residencies_) {
    if (res.model == model && res.model_generation != new_gen && res.is_resident_state()) {
      if (auto* d = domains_.find(res.domain)) { d->release(res.allocated_bytes); }
      if (backend_ && res.handle) { backend_->evict(res.cls, res.handle); res.handle = nullptr; }
      res.authority = AuthorityState::Stale;
      res.readiness = ReadinessState::Stale;
      try { res.transition_to(LifecycleState::Invalidated); } catch (const Error&) {}
    }
  }
  // Clear the active authoritative set for this model; a fresh load is required.
  auto as = active_set_by_model_.find(model);
  if (as != active_set_by_model_.end()) {
    sets_[as->second].ready = false;
    sets_[as->second].complete = false;
    active_set_by_model_.erase(as);
  }
}

// ---------------------------------------------------------------------------
// Rebalance
// ---------------------------------------------------------------------------
RebalancePlan Coordinator::rebalance(const AuthorityFrame&) {
  RebalancePlan plan;
  std::lock_guard<std::mutex> l(mutex_);
  for (const auto& [rid, res] : residencies_) {
    RebalanceAction act;
    act.residency = rid;
    std::string why;
    ModelGeneration cur = model_gens_.count(res.model) ? model_gens_[res.model] : ModelGeneration::zero();
    if (res.lifecycle == LifecycleState::Ready && res.model_generation == cur) {
      act.action = MovementAction::Keep;
      why = "ready and current";
    } else if (res.model_generation != cur) {
      act.action = MovementAction::Invalidate;
      why = "model generation is stale";
    } else if (res.is_resident_state() && res.active_references == 0 && !res.pinned &&
               res.lifecycle != LifecycleState::Migrating) {
      if (res.cls == ResidencyClass::AcceleratorLocal && device_pressure_locked()) {
        act.action = MovementAction::Demote;
        act.dest_class = ResidencyClass::PinnedHost;
        why = "device pressure; demote";
      } else {
        act.action = MovementAction::Keep;
        why = "resident and unreferenced";
      }
    } else {
      act.action = MovementAction::Keep;
      why = "in use";
    }
    act.reason = why;
    plan.actions.push_back(std::move(act));
  }
  plan.summary = std::string("rebalanced ") + std::to_string(plan.actions.size()) + " residencies";
  return plan;
}

// ---------------------------------------------------------------------------
// Inspection
// ---------------------------------------------------------------------------
const Residency* Coordinator::residency(ResidencyId id) const {
  std::lock_guard<std::mutex> l(mutex_);
  auto it = residencies_.find(id);
  return it == residencies_.end() ? nullptr : &it->second;
}

Json Coordinator::to_json() const {
  std::lock_guard<std::mutex> l(mutex_);
  Json root = Json::object();
  root.set("epoch", static_cast<std::uint64_t>(epoch_.value()));
  root.set("policy_generation", static_cast<std::uint64_t>(policy_gen_.value()));
  root.set("policy_fingerprint", policy_.fingerprint());
  Json models = Json::array();
  for (const auto& id : models_.model_ids()) {
    const ModelRevision* rev = models_.latest_revision(id);
    if (rev == nullptr) continue;
    Json j = Json::object();
    j.set("model", id.to_hex());
    j.set("revision", rev->revision.to_hex());
    j.set("version", rev->version);
    j.set("generation", static_cast<std::uint64_t>(model_gens_.count(id) ? model_gens_.at(id).value() : 0));
    models.push(std::move(j));
  }
  root.set("models", std::move(models));
  Json res = Json::array();
  for (const auto& [rid, r] : residencies_) {
    Json j = Json::object();
    j.set("id", rid.to_hex());
    j.set("model", r.model.to_hex());
    j.set("domain", r.domain.to_hex());
    j.set("class", residency_class_name(r.cls));
    j.set("lifecycle", lifecycle_state_name(r.lifecycle));
    j.set("readiness", readiness_state_name(r.readiness));
    j.set("authority", authority_state_name(r.authority));
    j.set("generation", static_cast<std::uint64_t>(r.generation.value()));
    j.set("bytes", static_cast<std::uint64_t>(r.byte_size.value()));
    res.push(std::move(j));
  }
  root.set("residencies", std::move(res));
  return root;
}

CoordinatorSnapshot Coordinator::snapshot() const {
  std::lock_guard<std::mutex> l(mutex_);
  CoordinatorSnapshot snap;
  snap.epoch = epoch_;
  snap.policy_generation = policy_gen_;
  for (const auto& id : models_.model_ids()) {
    const ModelRevision* rev = models_.latest_revision(id);
    if (rev != nullptr) { snap.models.push_back(*rev); }
  }
  snap.adapter_sets = adapter_sets_;
  for (const auto& did : domains_.ids()) {
    const MemoryDomain* d = domains_.find(did);
    if (d != nullptr) { snap.domains.push_back(*d); }
  }
  snap.sets.reserve(sets_.size());
  for (const auto& [sid, set] : sets_) { snap.sets.push_back(set); }
  snap.residencies.reserve(residencies_.size());
  for (const auto& [rid, r] : residencies_) {
    Residency copy = r;
    copy.handle = nullptr; // never persist live handles
    snap.residencies.push_back(std::move(copy));
  }
  return snap;
}

bool Coordinator::device_pressure_locked() const {
  for (const auto& did : domains_.ids()) {
    const MemoryDomain* d = domains_.find(did);
    if (d != nullptr && d->kind == MemoryDomainKind::Accelerator && d->governed_capacity.value() > 0) {
      if (d->available().value() < d->governed_capacity.value() / 2) { return true; }
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Snapshot serialization
// ---------------------------------------------------------------------------
Json CoordinatorSnapshot::to_json() const {
  Json root = Json::object();
  root.set("epoch", static_cast<std::uint64_t>(epoch.value()));
  root.set("policy_generation", static_cast<std::uint64_t>(policy_generation.value()));
  Json jsModels = Json::array();
  for (const auto& m : models) {
    Json j = Json::object();
    j.set("model", m.model.to_hex());
    j.set("revision", m.revision.to_hex());
    j.set("version", m.version);
    jsModels.push(std::move(j));
  }
  root.set("models", std::move(jsModels));
  Json jsRes = Json::array();
  for (const auto& r : residencies) {
    Json j = Json::object();
    j.set("id", r.id.to_hex());
    j.set("model", r.model.to_hex());
    j.set("domain", r.domain.to_hex());
    j.set("class", residency_class_name(r.cls));
    j.set("lifecycle", lifecycle_state_name(r.lifecycle));
    j.set("readiness", readiness_state_name(r.readiness));
    j.set("authority", authority_state_name(r.authority));
    j.set("generation", static_cast<std::uint64_t>(r.generation.value()));
    j.set("bytes", static_cast<std::uint64_t>(r.byte_size.value()));
    jsRes.push(std::move(j));
  }
  root.set("residencies", std::move(jsRes));
  return root;
}

} // namespace mr




