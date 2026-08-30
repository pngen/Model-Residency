# Examples

Run any example from a configured build tree:

```bat
build-release\examples\basic_residency.exe
build-release\examples\model_shards.exe
build-release\examples\adapter_residency.exe
build-release\examples\persistence_recovery.exe
build-release\src\cuda\cuda_residency_proof.exe   # requires the CUDA build
```

- **basic_residency** — define a model, load a residency on a device domain, prove warm reuse (no
  reload), then evict and verify capacity returns to baseline.
- **model_shards** — a two-shard model; the ResidencySet becomes complete and ready only when both
  required shards are resident.
- **adapter_residency** — attach a first-class adapter set to a base model and load it as part of the
  residency.
- **persistence_recovery** — save authoritative metadata and recover it into a fresh coordinator,
  demonstrating handle-free recovery and required readiness revalidation.
- **cuda_residency_proof** — the real cold-load → reuse → evict → reload device-memory proof.

The CLI (`mr_cli`) exercises `list models`, `list residencies`, `list sets`, `inspect state`,
`inspect capacity`, `load`, `evict`, `pin`, `unpin`, `invalidate`, `rebalance`, `roll`, `snapshot`,
`save`, `recover`, `migrate`, `demote`, and `run-demo`, with `--json` for machine output.
