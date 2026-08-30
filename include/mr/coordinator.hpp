#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "mr/config.hpp"
#include "mr/authority.hpp"
#include "mr/adapter.hpp"
#include "mr/compatibility.hpp"
#include "mr/backend.hpp"
#include "mr/capacity.hpp"
#include "mr/explain.hpp"
#include "mr/model.hpp"
#include "mr/placement.hpp"
#include "mr/policy.hpp"
#include "mr/residency.hpp"

namespace mr {

// ===========================================================================
// Worker / device registration
// ===========================================================================
struct WorkerRegistration {
  WorkerId worker = WorkerId::nil();
  WorkerBootId boot = WorkerBootId::nil();
  NodeId node = NodeId::nil();
  BackendType backend = BackendType::Unknown;
  std::uint32_t protocol_version = 1;
  std::vector<DeviceId> devices;
  std::vector<DeviceGeneration> device_generations;
};

// ===========================================================================
// Admission request / result
// ===========================================================================
struct AdmitRequest {
  ModelId model = ModelId::nil();
  ModelRevisionId model_revision = ModelRevisionId::nil();
  AdapterSetId adapter_set = AdapterSetId::nil();
  TenantId tenant = TenantId::nil();
  WorkloadId workload = WorkloadId::nil();
  std::uint32_t priority = 0;
  double expected_demand = 0;
  std::string latency_class = "default";
  bool allow_eviction = true;
  bool require_ready = true;
  bool partial_ok = false; // policy may permit partial residency
};

struct ReservationHandle {
  AttemptId attempt = AttemptId::nil();
  MemoryDomainId domain = MemoryDomainId::nil();
  Bytes amount{0};
  std::uint64_t counters_gen = 0; // monotonic reservation generation
  bool issued = false;
};

class AdmissionResult {
 public:
  AdmissionVerdict verdict = AdmissionVerdict::Reject;
  ResidencyId target_residency = ResidencyId::nil();
  PlacementDecision placement;
  AttemptId attempt = AttemptId::nil();
  ReservationHandle reservation;
  std::string reason;
  std::vector<ExplanationFactor> factors;
};

// ===========================================================================
// Outcomes
// ===========================================================================
class LoadOutcome {
 public:
  bool success = false;
  ResidencyId residency = ResidencyId::nil();
  ResidencySetId set = ResidencySetId::nil();
  Bytes loaded{0};
  std::string error;
  Explanation explanation;
};

struct MigrateRequest {
  ResidencyId residency = ResidencyId::nil();
  ResidencyClass dest_class = ResidencyClass::PinnedHost;
  DeviceId dest_device = DeviceId::nil();
  NodeId dest_node = NodeId::nil();
};

class MigrateOutcome {
 public:
  bool success = false;
  MigrationId migration = MigrationId::nil();
  ResidencyId dest_residency = ResidencyId::nil();
  Bytes bytes{0};
  std::string error;
  Explanation explanation;
};

struct EvictRequest {
  ResidencyId residency = ResidencyId::nil();
  EvictionReason reason = EvictionReason::CapacityPressure;
  bool force = false;
};

class EvictionOutcome {
 public:
  bool success = false;
  EvictionId eviction = EvictionId::nil();
  std::string error;
  Explanation explanation;
};

struct DemotionRequest {
  ResidencyId residency = ResidencyId::nil();
  ResidencyClass dest_class = ResidencyClass::PinnedHost;
};

class DemotionOutcome {
 public:
  bool success = false;
  std::string error;
  Explanation explanation;
};

// ===========================================================================
// Rebalance plan
// ===========================================================================
struct RebalanceAction {
  ResidencyId residency = ResidencyId::nil();
  MovementAction action = MovementAction::Keep;
  std::string reason;
  ResidencyClass dest_class = ResidencyClass::PinnedHost;
};

class RebalancePlan {
 public:
  std::vector<RebalanceAction> actions;
  std::string summary;
  Json to_json() const;
  std::string to_text() const;
};

// ===========================================================================
// Snapshot
// ===========================================================================
/// An immutable, self-contained view of authoritative coordinator state. No live
/// pointers, handles, or device pointers are captured. Used for persistence and
/// for race-free read-side inspection.
class CoordinatorSnapshot {
 public:
  CoordinatorEpoch epoch = CoordinatorEpoch::zero();
  PolicyGeneration policy_generation = PolicyGeneration::zero();
  std::vector<ModelRevision> models;
  std::vector<AdapterSet> adapter_sets;
  std::vector<MemoryDomain> domains;
  std::vector<Residency> residencies; // handles are always nulled
  std::vector<ResidencySet> sets;
  Json to_json() const;
};

// ===========================================================================
// Coordinator
// ===========================================================================
/// The authoritative coordinator for residency lifecycle and placement. It owns
/// every residency object, the capacity accounting, the authority state, and
/// the guarded lifecycle transitions.
///
/// Concurrency contract: the coordinator holds a single master mutex for its
/// state. It NEVER calls the backend (which may do blocking CUDA, file, or
/// network work) while holding that mutex. Heavy operations release the lock,
/// perform the byte movement, then re-acquire and atomically verify + commit.
class Coordinator {
 public:
  explicit Coordinator(ResidencyBackend* backend, ResidencyPolicy policy);

  // --- registration ---
  void define_model(ModelRevision revision);
  void register_adapter_set(AdapterSet set);
  void register_domain(MemoryDomain domain);
  void register_worker(WorkerRegistration reg);
  void mark_worker_lost(WorkerId worker, const AuthorityFrame& a);

  // --- decisions & lifecycle ---
  [[nodiscard]] AdmissionResult admit(const AdmitRequest& req, const AuthorityFrame& a);
  LoadOutcome load(const AdmitRequest& req, const AuthorityFrame& a);
  void publish_ready(ResidencyId id, const AuthorityFrame& a);
  MigrateOutcome migrate(const MigrateRequest& req, const AuthorityFrame& a);
  EvictionOutcome evict(const EvictRequest& req, const AuthorityFrame& a);
  DemotionOutcome demote(const DemotionRequest& req, const AuthorityFrame& a);
  void pin(ResidencyId id);
  void unpin(ResidencyId id);
  void invalidate(ResidencyId id, const AuthorityFrame& a);
  void roll_model_generation(ModelId model, ModelRevision new_revision, const AuthorityFrame& a);
  RebalancePlan rebalance(const AuthorityFrame& a);

  // --- inspection ---
  [[nodiscard]] const Residency* residency(ResidencyId id) const;
  [[nodiscard]] const ModelCatalog& models() const noexcept { return models_; }
  [[nodiscard]] const std::map<ResidencyId, Residency>& residencies() const noexcept { return residencies_; }
  [[nodiscard]] const std::map<ResidencySetId, ResidencySet>& sets() const noexcept { return sets_; }
  [[nodiscard]] const MemoryDomainPool& domains() const noexcept { return domains_; }
  [[nodiscard]] CoordinatorEpoch epoch() const noexcept;
  [[nodiscard]] PolicyGeneration policy_generation() const noexcept;

  [[nodiscard]] Json to_json() const;
  [[nodiscard]] CoordinatorSnapshot snapshot() const;

 private:
  mutable std::mutex mutex_;
  ResidencyBackend* backend_;
  ResidencyPolicy policy_;
  CoordinatorEpoch epoch_ = CoordinatorEpoch::first();
  PolicyGeneration policy_gen_ = PolicyGeneration::first();
  ModelCatalog models_;
  std::vector<AdapterSet> adapter_sets_;
  MemoryDomainPool domains_;
  std::map<ResidencyId, Residency> residencies_;
  std::map<ResidencySetId, ResidencySet> sets_;
  std::map<ModelId, ResidencySetId> active_set_by_model_;
  std::map<WorkerId, WorkerRegistration> workers_;
  StaleAuthorityChecker stale_;
  std::map<ModelId, ModelGeneration> model_gens_;
  std::map<DeviceId, DeviceGeneration> device_gens_;
  std::uint64_t next_generation_counter_ = 1;

  ResidencyGeneration next_residency_gen_locked() { return ResidencyGeneration(next_generation_counter_++); }
  bool device_pressure_locked() const;
};

} // namespace mr
