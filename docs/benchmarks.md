# Benchmarks

`mr_bench` reports completed operations and real transferred bytes. It covers: model identity
construction, compatibility decisions, residency lookup, admission decisions, placement, capacity
reservation, lifecycle transitions, snapshot generation, explanation generation, persistence
encode/decode, large residency sets, 1/4/8-thread lookup, concurrent lifecycle churn, and (where a CUDA
build is used) real CUDA cold load / warm reuse / device eviction / reload.

Run:

```bat
build-release\bench\mr_bench.exe
```

All counts are real completed operations and real byte totals; synchronization and durability
boundaries are not hidden.
