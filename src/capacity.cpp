#include "mr/capacity.hpp"

#include <algorithm>

namespace mr {

Bytes MemoryDomain::available() const {
  Bytes after_reserve = checked_sub(governed_capacity, reserved_);
  return checked_sub(after_reserve, resident_);
}

bool MemoryDomain::can_reserve(Bytes amount) const {
  if (amount.value() == 0) return true;
  const std::uint64_t used = reserved_.value() + resident_.value();
  if (used < reserved_.value()) return false; // overflow guard
  // used + amount <= governed
  if (amount.value() > governed_capacity.value()) return false;
  return amount.value() <= governed_capacity.value() - used;
}

bool MemoryDomain::can_resident(Bytes amount) const {
  if (amount.value() == 0) return true;
  if (amount.value() > governed_capacity.value()) return false;
  return amount.value() <= governed_capacity.value() - resident_.value();
}

bool MemoryDomain::reserve(Bytes amount) {
  if (!can_reserve(amount)) return false;
  reserved_ = checked_add(reserved_, amount);
  return true;
}

void MemoryDomain::commit(Bytes amount) {
  throw_if(amount.value() > reserved_.value(), ErrorCode::InvalidState,
           "commit exceeds outstanding reservation");
  // reserved_ - amount then resident + amount
  throw_if(!can_resident(amount), ErrorCode::CapacityExceeded,
           "commit would exceed governed capacity");
  reserved_ = checked_sub(reserved_, amount);
  resident_ = checked_add(resident_, amount);
}

void MemoryDomain::cancel_reservation(Bytes amount) {
  throw_if(amount.value() > reserved_.value(), ErrorCode::InvalidState,
           "cancel exceeds outstanding reservation");
  reserved_ = checked_sub(reserved_, amount);
}

void MemoryDomain::release(Bytes amount) {
  throw_if(amount.value() > resident_.value(), ErrorCode::InvalidState,
           "release exceeds resident bytes (double release or negative accounting)");
  const Bytes pinned_reduction = Bytes(std::min(amount.value(), pinned_.value()));
  pinned_ = checked_sub(pinned_, pinned_reduction);
  resident_ = checked_sub(resident_, amount);
}

bool MemoryDomain::pin(Bytes amount) {
  const std::uint64_t reclaimable = resident_.value() - pinned_.value();
  if (amount.value() > reclaimable) return false;
  pinned_ = checked_add(pinned_, amount);
  return true;
}

void MemoryDomain::unpin(Bytes amount) {
  throw_if(amount.value() > pinned_.value(), ErrorCode::InvalidState,
           "unpin exceeds pinned bytes");
  pinned_ = checked_sub(pinned_, amount);
}

void MemoryDomain::set_unavailable(Bytes amount) {
  throw_if(amount.value() > total_capacity.value(), ErrorCode::InvalidArgument,
           "unavailable exceeds total capacity");
  unavailable_capacity = amount;
  // Governed capacity is the part MR may manage: total minus unavailable.
  if (governed_capacity.value() == 0) {
    governed_capacity = amount.value() >= total_capacity.value() ? Bytes(0)
                                                                 : Bytes(total_capacity.value() - amount.value());
  }
}

void MemoryDomain::verify_invariants() const {
  throw_if(pinned_.value() > resident_.value(), ErrorCode::InvalidState,
           "pinned exceeds resident");
  const std::uint64_t used = reserved_.value() + resident_.value();
  throw_if(used < reserved_.value() || used > governed_capacity.value(), ErrorCode::InvalidState,
           "overcommit: reserved+resident exceeds governed capacity");
  throw_if(unavailable_capacity.value() > total_capacity.value(), ErrorCode::InvalidState,
           "unavailable exceeds total");
}

MemoryDomain& MemoryDomainPool::register_domain(MemoryDomain domain) {
  throw_unless(!domain.id.is_nil(), ErrorCode::InvalidIdentity, "domain id must be set");
  for (auto& existing : domains_) {
    if (existing.id == domain.id) {
      throw_if(existing.resident_.value() != 0 || existing.reserved_.value() != 0,
               ErrorCode::InvalidState, "cannot overwrite a domain with active accounting");
      existing.kind = domain.kind;
      existing.device = domain.device;
      existing.node = domain.node;
      existing.backend = domain.backend;
      existing.capability = domain.capability;
      existing.total_capacity = domain.total_capacity;
      existing.governed_capacity = domain.governed_capacity;
      existing.unavailable_capacity = domain.unavailable_capacity;
      existing.generation = domain.generation;
      return existing;
    }
  }
  domains_.push_back(std::move(domain));
  return domains_.back();
}

const MemoryDomain* MemoryDomainPool::find(MemoryDomainId id) const {
  for (const auto& d : domains_) { if (d.id == id) return &d; }
  return nullptr;
}

MemoryDomain* MemoryDomainPool::find(MemoryDomainId id) {
  for (auto& d : domains_) { if (d.id == id) return &d; }
  return nullptr;
}

const MemoryDomain* MemoryDomainPool::find_device(DeviceId device, MemoryDomainKind kind) const {
  for (const auto& d : domains_) { if (d.device == device && d.kind == kind) return &d; }
  return nullptr;
}

MemoryDomain* MemoryDomainPool::find_device(DeviceId device, MemoryDomainKind kind) {
  for (auto& d : domains_) { if (d.device == device && d.kind == kind) return &d; }
  return nullptr;
}

std::vector<MemoryDomainId> MemoryDomainPool::ids() const {
  std::vector<MemoryDomainId> out;
  for (const auto& d : domains_) { out.push_back(d.id); }
  return out;
}

} // namespace mr
