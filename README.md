# Model Residency

**Version 1.0.0** · C++20 · CMake · Windows-first · MSVC `/W4 /WX` · CUDA 13.1 / RTX 5090 (sm_120)

Model Residency is a production-grade, open-source, vendor-neutral C++20 runtime that governs the
**active placement and lifecycle of model weights and adapters** across heterogeneous AI serving
infrastructure.

## The core question

> **Where should active model weights and adapters reside, which copies are valid and ready, when
> should they move, and when must residency be changed because capacity, compatibility, demand,
> topology, or authority changed?**

---

## Systems boundary

Model Residency is deliberately **not** Model Cache, Warmth Fabric, Replica Fabric, or an Inference
Scheduler. It is the layer that answers one question: *where is the active model state physically
loaded right now, which copy is valid and execution-ready, and when should that residency change?*

| System            | Question it answers                                            |
|-------------------|----------------------------------------------------------------|
| **Model Cache**   | What reusable model artifact exists?                           |
| **Model Residency** | Where is the active model state loaded now, which copy is valid and ready, and when should residency change? |
| **Warmth Fabric** | How prepared is the surrounding runtime to execute useful work immediately? |
| **Replica Fabric**| Which live serving replica is authoritative and allowed to serve? |

A model may be **resident but not fully warm**. A model may be **warm because all required residency
and runtime preparation already exists**. Model Residency preserves that distinction — it never
claims execution readiness for a residency class that cannot execute the workload.

---

## Highlights

- **Strong 128-bit typed identities** for every entity, so a `ModelId` can never be silently used as
  a `NodeId` or `AdapterId`.
- **Explicit, guarded residency lifecycle** with deterministic invalid-transition rejection.
- **Exact capacity accounting** that prevents over-allocation, double reservation, double release,
  negative accounting, overflow, and silent drift.
- **Real framed-TCP distributed coordinator + worker processes** with an atomic restart proof.
- **Real CUDA residency proof** on the RTX 5090: cold load → real kernel → reuse → evict → reload,
  with exact device-memory recovery.
- **Versioned, checksummed persistence** with strict recovery validation.

---

## Build, install, and use

### Prerequisites

- CMake ≥ 3.24, Ninja
- MSVC 2022 (or Clang/GCC on another platform)
- CUDA 13.1 with an sm_120 device (RTX 5090) for the CUDA proof; CUDA is optional otherwise

### Configure and build

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
```

Options: `MR_ENABLE_CUDA`, `MR_BUILD_TESTS`, `MR_BUILD_BENCH`, `MR_BUILD_EXAMPLES`, `MR_BUILD_CLI`,
`MR_BUILD_DISTRIBUTED`.

### Test

The test suite has **no timeouts** anywhere: no `timeoutMs`, no CTest `TIMEOUT`, no watchdogs. Tests run
to natural completion.

```bat
build-release\test\mr_tests.exe
build-release\distributed\multiprocess_proof.exe ^
  build-release\distributed\mr_coordinator.exe build-release\distributed\mr_worker.exe
```

### Install and consume

```bat
cmake --install build-release --prefix build-release/install
cd consumer
cmake -S . -B build-consumer -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH="<prefix>"
cmake --build build-consumer --config Release
build-consumer\consumer.exe
```

The consumer uses `find_package(ModelResidency CONFIG REQUIRED)` and links `ModelResidency::mr_core`.

---

## Identity

Model identity is explicit. A `ModelRevision` carries `ModelId`, `ModelRevisionId`, `ArtifactId`,
`ArtifactGeneration`, format, numeric mode, quantization, tokenizer/vocabulary identity,
architecture/family, backend and device compatibility, required memory, shard topology, and a content
digest where available. A bare name string is never sufficient identity.

Residencies, adapter sets, shards, placements, tenants, workloads, nodes, devices, workers, attempts,
migrations, evictions, and policies all have distinct 128-bit identity types (see
`include/mr/core/identity.hpp`) and distinct generation types (see `include/mr/core/generation.hpp`).

---

## Residency lifecycle

`DECLARED → PLANNED → ALLOCATING → LOADING → VALIDATING → RESIDENT → READY → {MIGRATING | DEMOTING |
EVICTING | INVALIDATED} → EVICTED → RETIRED`, plus `FAILED`.

Transitions are guarded and invalid transitions fail deterministically (`ErrorCode::InvalidTransition`).

A residency becomes **execution-ready** only when lifecycle == `READY`, validation == `valid`,
compatibility == `compatible`, authority == `authoritative`, readiness == `ready`, the residency
generation is current, and the residency class can actually execute. Partial residency is represented
explicitly and is never treated as complete.

---

## Shards and adapters

- A model may be composed of required `ShardId`s; model readiness requires every required shard,
  unless policy explicitly permits partial execution.
- Adapters are first-class objects with their own generation and a base-model relationship. A stale
  adapter generation is never silently attached to a newer base-model generation.

---

## Compatibility

Compatibility is typed and inspectable across model id / revision, artifact generation, adapter set,
tokenizer/vocabulary, backend, runtime, architecture, compute capability, datatype, quantization,
layout, shard structure, and policy fingerprint. It is never inferred from filenames or free-form
strings (`include/mr/compatibility.hpp`).

---

## Capacity

Each memory domain tracks total, governed, reserved, allocated (resident), pinned/protected,
reclaimable, and unavailable bytes. Accounting is exact and guarded; teardown returns every domain to
its baseline with zero drift.

---

## Placement and explainability

Placement decisions expose the constituent factors (device capability, memory, model/shard/adapter
sizes, existing residency, transfer cost, topology, locality, demand, active references, latency
class, priority, tenant constraints, failure domain, memory pressure, competing residency, migration
cost, eviction cost, reload cost, policy generation) and a deterministic tie-break — never one opaque
score. Every major decision (admission, load, rejection, defer, migration, demotion, eviction,
rebalancing, invalidation, reload) is inspectable.

---

## Loading, migration, demotion, eviction, generation rollover

- **Loading** is transactional: plan → reserve → locate/validate artifact → allocate → transfer →
  verify → publish → commit authority. On failure the reservation is released and prior valid
  residency is preserved; partial success is never published.
- **Migration** is transactional; the last valid source copy is preserved until destination validation
  completes (unless policy explicitly allows destructive migration).
- **Demotion** preserves identity and generation; a demoted copy remains reusable without being
  execution-ready.
- **Eviction** is safe: a residency is never evicted if it has active references, a protected lease,
  an in-flight dependency, is the only required authoritative copy under policy, is mid-migration, or
  has ambiguous generation state.
- **Generation rollover** never silently replaces current residency: a fresh residency is validated and
  published, and the old generation is retired — old and new remain distinguishable.

---

## Distributed authority

A resident copy is authoritative only when coordinator epoch, worker boot id, residency/model/artifact/
adapter/device/policy generations all agree. Stale messages (checked in strict generation order) never
publish residency, mark residency ready, complete a migration, release capacity, evict a fresh
residency, overwrite a model generation, restore stale adapter state, or resurrect process-local
authority.

The reference implementation uses a real **coordinator OS process** plus **worker OS processes** over
**framed TCP**. The `mr_multiprocess_restart` proof kills a worker, restarts it under a fresh boot id,
replays stale boot/generation messages, and proves they are rejected while fresh work under new
authority completes with no duplicate residency and no capacity leak.

---

## Persistence / recovery

Authoritative metadata is persisted with versioned, checksummed binary encoding. On recovery, durable
metadata survives but live process/device residency is **revalidated** — worker restart never inherits
readiness automatically, device allocations are not presumed valid, and missing workers retain no
serving authority. Malformed lengths, truncation, duplicate IDs, invalid enums, impossible
transitions, invalid generation relations, checksum corruption, NaN, Inf, integer overflow, trailing
garbage, and incompatible versions are all rejected.

---

## CUDA proof

`mr_cuda` and the `cuda_residency_proof` example exercise a real RTX 5090 / CUDA 13.1 / sm_120 path:
baseline device memory → deterministic host weights → device allocation → transfer → verify → publish
residency → run a real CUDA kernel → compare to CPU reference → retain → **reuse** → **evict** (verify
memory recovery) → **reload** under a new residency generation → re-execute → teardown → verify exact
device-memory recovery to baseline.

---

## Synthetic multi-device validation

Only one physical GPU is available. Tensor-parallel-like sharding, multi-device placement, replica
residency, cross-device movement planning, failure-domain placement, and rebalancing are validated
**synthetically** and are explicitly labeled as such. No physical multi-GPU, NVLink, RDMA, DPU, or CXL
behavior is claimed.

---

## Testing

Coverage includes strong identity, model revision identity, artifact generation, adapter identity,
compatibility, residency and ResidencySet lifecycle, invalid transitions, partial residency, complete
residency, capacity accounting, admission, loading, failed-load rollback, duplicate load, concurrent
load, placement, migration, demotion, eviction, protected/active-reference protection, rebalancing,
generation rollover, model/update, adapter update, device loss, worker restart, stale epoch/boot/
generation, persistence, recovery, truncation/corruption rejection, malformed protocol frames,
concurrency, deterministic replay, resource accounting, CUDA load/reuse/evict/reload, CLI, and a
downstream find_package consumer.

Property tests (fixed seed) and a high-contention randomized residency churn loop continuously assert
invariants (exact accounting, no duplicate authoritative publication, no leak).

---

## Benchmarks

`mr_bench` reports completed operations and real transferred bytes for identity construction,
compatibility decisions, residency lookup, admission, placement, capacity reservation, lifecycle
transitions, snapshot generation, persistence encode/decode, concurrent lifecycle churn, and real
(where applicable) CUDA cold load / warm reuse / eviction / reload.

---

## License

Copyright 2026 Summon Software Labs. Apache License 2.0. See `LICENSE`.
