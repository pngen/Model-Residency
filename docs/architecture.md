# Architecture

Model Residency 1.0.0 is a C++20 library plus a set of executables. The core library
(`mr_core`) is vendor-neutral and has no required dependency on CUDA or any other optional
subsystem. CUDA, the framed-TCP distributed runtime, the CLI, examples, and benchmarks are optional
layers over the core.

## Layers

1. **Core primitives** (`mr/core`): 128-bit identities (`Uid128`), strongly typed identity and generation
   types, enums + safe converters, exact byte accounting, bounded binary serialization, deterministic
   JSON, error/result types, verified checksums.
2. **Domain model** (`mr/model.hpp`, `mr/adapter.hpp`, `mr/compatibility.hpp`): model revisions, artifacts,
   shards, adapters, adapter sets, and typed compatibility checks.
3. **State** (`mr/capacity.hpp`, `mr/residency.hpp`, `mr/policy.hpp`, `mr/authority.hpp`): memory domains with
   exact accounting, guards, the guarded lifecycle state machine, ResidencySet, policy, and stale
   authority validation.
4. **Decisions** (`mr/placement.hpp`, `mr/explain.hpp`): placement candidates, decisions, and structured,
   inspectable explanations.
5. **Runtime** (`mr/coordinator.hpp`): the authoritative coordinator that owns admission, transactional
   loading, atomic publication, migration, demotion, eviction, rebalancing, generation rollover,
   device-loss handling, and worker registration. Heavy backend work is never performed while the
   master lock is held.
6. **Persistence** (`mr/persistence.hpp`): versioned, checksummed binary encode/decode with strict
   recovery validation.
7. **Network** (`mr/net.hpp`, `mr/protocol.hpp`): bordered TCP streams and framed messages used by the
   distributed coordinator + worker processes.
8. **Backends** (`mr/backends/*`): `MemoryBackend` (deterministic, hardware-free) and `CudaBackend`
   (real device memory, RTX 5090 / sm_120).

## Concurrency contract

The coordinator holds a single master mutex for its state. It never performs blocking CUDA,
persistence, artifact-loading, transfer, or network work while holding that lock. Heavy operations
release the lock, do the byte movement, then re-acquire and atomically verify + commit. This
eliminates load/evict/migration races, lock-order inversion, callbacks under the lock, and stale CUDA
pointer use.

## Authority model

Every residency carries its coordinator epoch, worker boot id, residency/model/artifact/device/policy
generations. Worker→coordinator messages are validated in strict generation order
(coordinator epoch → worker boot → residency → model → artifact → adapter → device → attempt/migration).
A stale message cannot publish, mark ready, complete a migration, release capacity, evict, overwrite,
or resurrect authority.
