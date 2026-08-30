#include "mr_test.hpp"

#include "mr/core/byte_size.hpp"
#include "mr/core/enums.hpp"
#include "mr/core/identity.hpp"
#include "mr/core/uid.hpp"
#include "mr/authority.hpp"
#include "mr/capacity.hpp"
#include "mr/compatibility.hpp"
#include "mr/model.hpp"
#include "mr/residency.hpp"

MR_TEST(uid_basic) {
  auto a = mr::Uid128::random();
  auto b = mr::Uid128::random();
  MR_ASSERT(a != b);
  MR_ASSERT(!a.is_nil());
  MR_ASSERT(mr::Uid128::nil().is_nil());
  MR_ASSERT(mr::Uid128::from_hex(a.to_hex()) == a);
  MR_ASSERT(mr::Uid128::from_hex(a.to_canonical()) == a);
}

MR_TEST(identity_distinct) {
  mr::ModelId m = mr::ModelId::random();
  mr::NodeId n = mr::NodeId::random();
  // Distinct tags produce distinct types; assignment across types is rejected at
  // compile time. Here we merely check they are independent values.
  MR_ASSERT(!m.is_nil() && !n.is_nil());
  MR_ASSERT(m != mr::ModelId::nil());
  MR_ASSERT(mr::ModelId::from_hex(m.to_hex()) == m);
}

MR_TEST(generation_monotonic) {
  mr::ModelGeneration g = mr::ModelGeneration::first();
  MR_ASSERT(g.value() == 1);
  auto g2 = g.next();
  MR_ASSERT(g2 > g);
  MR_ASSERT(mr::CoordinatorEpoch::zero().is_zero());
}

MR_TEST(bytes_accounting_exact) {
  mr::MemoryDomain d;
  d.id = mr::MemoryDomainId::random();
  d.kind = mr::MemoryDomainKind::Accelerator;
  d.total_capacity = mr::Bytes::from_mib(100);
  d.governed_capacity = mr::Bytes::from_mib(100);
  MR_ASSERT(d.available() == d.governed_capacity);

  MR_ASSERT(d.reserve(mr::Bytes::from_mib(30)) == true);
  MR_ASSERT(d.reserved() == mr::Bytes::from_mib(30));

  d.commit(mr::Bytes::from_mib(30));
  MR_ASSERT(d.resident() == mr::Bytes::from_mib(30));
  MR_ASSERT(d.available() == mr::Bytes::from_mib(70));

  // Over-reserve beyond capacity is refused.
  MR_ASSERT(d.reserve(mr::Bytes::from_mib(80)) == false);

  // Double release is rejected (negative accounting guard).
  bool threw = false;
  try { d.release(mr::Bytes::from_mib(50)); } catch (const mr::Error&) { threw = true; }
  MR_ASSERT(threw);

  d.release(mr::Bytes::from_mib(30));
  MR_ASSERT(d.available() == d.governed_capacity);
  d.verify_invariants();
}

MR_TEST(bytes_pinned_reclaimable) {
  mr::MemoryDomain d;
  d.total_capacity = mr::Bytes(100);
  d.governed_capacity = mr::Bytes(100);
  d.reserve(mr::Bytes(60));
  d.commit(mr::Bytes(60));
  MR_ASSERT(d.reclaimable() == mr::Bytes(60));
  MR_ASSERT(d.pin(mr::Bytes(40)) == true);
  MR_ASSERT(d.pinned() == mr::Bytes(40));
  MR_ASSERT(d.reclaimable() == mr::Bytes(20));
  MR_ASSERT(d.pin(mr::Bytes(50)) == false); // exceeds reclaimable
  d.unpin(mr::Bytes(40));
  MR_ASSERT(d.reclaimable() == mr::Bytes(60));
  d.release(mr::Bytes(60));
  d.verify_invariants();
}

MR_TEST(compatibility_checks) {
  mr::ModelRevision m;
  m.model = mr::ModelId::random();
  m.revision = mr::ModelRevisionId::random();
  m.artifact.id = mr::ArtifactId::random();
  m.artifact.generation = mr::ArtifactGeneration::first();
  m.artifact.backend = mr::BackendType::Cuda;
  m.artifact.capability = mr::ComputeCapability::Sm_120;
  m.backend_compat = {mr::BackendType::Cuda};
  m.device_compat = {mr::ComputeCapability::Sm_120};

  mr::CompatibilityChecker c;
  MR_ASSERT(c.check_model_backend(m, mr::BackendType::Cuda).compatible());
  MR_ASSERT(c.check_model_backend(m, mr::BackendType::Cpu).incompatible());
  MR_ASSERT(c.check_model_device(m, mr::ComputeCapability::Sm_120).compatible());
  MR_ASSERT(c.check_model_device(m, mr::ComputeCapability::Sm_90).incompatible());
  MR_ASSERT(c.check_datatype(mr::DataType::Fp16, mr::DataType::Fp16).compatible());
  MR_ASSERT(c.check_datatype(mr::DataType::Fp16, mr::DataType::Fp32).incompatible());
}

MR_TEST(authority_stale_order) {
  mr::StaleAuthorityChecker chk;
  mr::AuthorityFrame current;
  current.coordinator_epoch = mr::CoordinatorEpoch(5);
  current.worker_boot = mr::WorkerBootId::random();
  current.residency_generation = mr::ResidencyGeneration(7);
  current.model_generation = mr::ModelGeneration(3);
  current.artifact_generation = mr::ArtifactGeneration(4);
  current.adapter_generation = mr::AdapterGeneration(2);
  current.device_generation = mr::DeviceGeneration(6);
  current.policy_generation = mr::PolicyGeneration(1);

  mr::AuthorityFrame msg = current;
  MR_ASSERT(chk.validate(msg, current, true) == mr::StaleReason::None);

  msg = current;
  msg.coordinator_epoch = mr::CoordinatorEpoch(4);
  MR_ASSERT(chk.validate(msg, current, true) == mr::StaleReason::CoordinatorEpoch);

  msg = current;
  msg.residency_generation = mr::ResidencyGeneration(6);
  MR_ASSERT(chk.validate(msg, current, true) == mr::StaleReason::ResidencyGeneration);

  msg = current;
  msg.model_generation = mr::ModelGeneration(2);
  MR_ASSERT(chk.validate(msg, current, true) == mr::StaleReason::ModelGeneration);

  msg = current;
  msg.worker_boot = mr::WorkerBootId::random();
  MR_ASSERT(chk.validate(msg, current, true) == mr::StaleReason::WorkerBoot);
}

MR_TEST(residency_lifecycle_guard) {
  mr::Residency r;
  r.lifecycle = mr::LifecycleState::Declared;
  r.transition_to(mr::LifecycleState::Planned);
  r.transition_to(mr::LifecycleState::Allocating);
  r.transition_to(mr::LifecycleState::Loading);
  r.transition_to(mr::LifecycleState::Validating);
  r.transition_to(mr::LifecycleState::Resident);
  r.transition_to(mr::LifecycleState::Ready);
  MR_ASSERT(r.is_execution_ready() == false); // state qualifiers not authoritative

  // Invalid transition: Ready -> Declared must throw.
  bool threw = false;
  try { r.transition_to(mr::LifecycleState::Declared); } catch (const mr::Error&) { threw = true; }
  MR_ASSERT(threw);

  r.validation = mr::ValidationState::Valid;
  r.compatibility = mr::CompatibilityState::Compatible;
  r.authority = mr::AuthorityState::Authoritative;
  r.readiness = mr::ReadinessState::Ready;
  r.generation = mr::ResidencyGeneration(1);
  r.cls = mr::ResidencyClass::ProcessLocal;
  MR_ASSERT(r.is_execution_ready());
  MR_ASSERT(!mr::residency_class_is_execution_ready(mr::ResidencyClass::PageableHost));
}
