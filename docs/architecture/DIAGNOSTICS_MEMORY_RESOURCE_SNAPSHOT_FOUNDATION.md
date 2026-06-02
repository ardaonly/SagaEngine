# Diagnostics Memory Resource Snapshot Foundation

Phase 5 adds the first `SagaDiagnostics` memory/resource snapshot foundation.

## Scope

- `MemoryTracker` records explicit per-system counters only.
- `MemorySnapshot` captures deterministic records for current bytes, peak
  bytes, total allocated bytes, total freed bytes, and active explicit
  allocation count.
- `MemoryScope` provides a small RAII wrapper around explicit tracker calls.
- `ResourceTracker` records explicitly registered non-memory resources.
- `ResourceSnapshot` captures active or all resource records in deterministic
  order and can build active leak-candidate summaries.
- `DiagnosticSystem` owns memory and resource trackers and includes their
  snapshots in operational, crash, and reliability reports.
- `SagaStressArena` writes deterministic fake/explicit memory and resource
  records into its direct local ZoneServer reports.

The tracked resource types are socket, file, timer, job, asset handle, database
connection, thread, and other. These are explicit diagnostic labels, not OS
handle enumeration.

## Report Shape

Operational reports and crash/reliability reports now include:

- `memoryRecords`
- `resourceSummary`
- `activeResources`
- summary counts for memory systems and active resources

Missing memory/resource data is deterministic: reports contain empty arrays and
zero summary counts.

## Dependency Direction

```txt
Core must not depend on Diagnostics.
SagaDiagnostics may depend on Core/CoreLog and privately on nlohmann_json.
Server and Tools may depend on SagaDiagnostics at their owning boundaries.
SagaDiagnostics must not depend on Server, Runtime, Editor, Product, Tools, or SDE.
```

The existing Core `MemoryTracker`, `ArenaAllocator`, and replication memory
tracker are not connected to this Phase 5 report path. That avoids turning this
phase into allocator replacement or production leak detection work.

## SagaStressArena Integration

`SagaStressArena` still uses the direct local ZoneServer harness. It now records
small deterministic memory counters for `StressArena`, `ZoneServer`, and
`Movement`, plus explicit resource records for the scenario runner and direct
harness.

This proves memory/resource snapshot integration in report artifacts. It is not
real OS memory profiling and it is not a real resource leak detector.

## Non-Claims

Phase 5 non-claims: no custom allocator, no operator new/delete override, no
allocator replacement, no raw allocation interception, no stack traces per
allocation, no OS memory polling, no OS handle enumeration, no real socket/file
scanning, no full leak detection, no production memory safety claim, no
NetworkChaos, no StateValidation, no FaultBoundary, no remote telemetry, no
SDE-driven diagnostics config, no production readiness, and no full raw CTest
health claim.
