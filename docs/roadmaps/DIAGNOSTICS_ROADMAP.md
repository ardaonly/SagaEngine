# Saga Diagnostics Roadmap

> Last updated: 2026-05-26
> Status: Active roadmap
> Target: A production-grade diagnostics system that gives Saga product, editor, runtime, server, tools, build pipeline, collaboration, asset pipeline, scripting, SDE, Forge, and Prism a shared diagnostic language without collapsing their implementation ownership.
> Scope: Diagnostic contracts, severity model, source/resource locations, graph/script/asset/package references, runtime/server diagnostics, build/publish diagnostics, collaboration diagnostics, Prism reports, health snapshots, structured reports, editor navigation, product summaries, CI artifacts, crash/shutdown context, and long-running server reliability diagnostics.

---

## 0. Roadmap Convention

* `[x]` — Shipped. The note after the item names concrete files, modules, tests, reports, tool outputs, or integration points that represent completed work.
* `[ ]` — Open. The item describes the finished production state, not temporary scaffolding.
* Shipped diagnostics work must include contract/report formats, emitting systems, consuming systems, and tests where practical.
* Open diagnostics work must describe observable diagnostic behavior, not vague logging ambition.
* Diagnostics must be structured.
* Diagnostics must be source/resource linked where practical.
* Diagnostics must be machine-readable for tools and CI.
* Diagnostics must be human-readable for editor/product users.
* Diagnostics presentation may change by authoring profile.
* Diagnostics truth must not change by authoring profile.
* Diagnostics must not make `SagaShared` own implementation.
* Diagnostics must not make the editor own runtime/server/tool truth.

A diagnostic system that only prints strings is not diagnostics.

It is a terminal confessional.

---

## 1. Document Purpose

This document defines Saga's diagnostics roadmap.

Diagnostics are the connective tissue between Saga's many subsystems:

```txt
Saga product shell
SagaEditor
Runtime
Server
SagaShared contracts
SagaCollaboration
SDE
Forge
Prism
AssetPipeline
Scripting Toolchain
Package/Publish pipeline
CI
```

The goal is not one giant diagnostics implementation that every system depends on.

The goal is a shared diagnostic language and report structure that lets each subsystem emit precise evidence while preserving ownership boundaries.

Correct model:

```txt
Subsystem emits structured diagnostics
    ↓
Diagnostics are normalized into shared contracts/reports where useful
    ↓
Saga / Editor / Forge / CI / Runtime tools consume and present them
    ↓
User can navigate to exact source/resource/artifact/build issue
```

Incorrect model:

```txt
Everything logs a string and the editor regexes terminal output.
```

That is not an architecture.

That is archaeology with ANSI colors.

---

## 2. Companion Documents

| Document                            | Purpose                                                                                |
| ----------------------------------- | -------------------------------------------------------------------------------------- |
| `SHARED_ROADMAP.md`                 | Shared diagnostic contracts and payload descriptors                                    |
| `SAGA_PRODUCT_ROADMAP.md`           | Product diagnostic summaries, publish blockers, build state presentation               |
| `EDITOR_ROADMAP.md`                 | Problems panel, Diagnostics Report panel, source/resource navigation                   |
| `ENGINE_ROADMAP.md`                 | Runtime/server diagnostics, package/artifact validation, networking/replication health |
| `COLLABORATION_ROADMAP.md`          | Collaboration diagnostics for sessions, permissions, locks, conflicts, publish gates   |
| `SDE_ROADMAP.md`                    | Compiler diagnostics, source maps, artifact manifests                                  |
| `FORGE_ROADMAP.md`                  | Build diagnostics, report aggregation, publish readiness reports                       |
| `PRISM_ROADMAP.md`                  | Boundary/stale/artifact relationship reports                                           |
| `ASSET_PIPELINE_ROADMAP.md`         | Asset import/cook/manifest/runtime asset diagnostics                                   |
| `SAGA_SCRIPTING_ROADMAP.md`         | Script compile, binding, authority metadata diagnostics                                |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md`    | Graph/node/pin/edge diagnostics                                                        |
| `AUTHORING_AUTHORITY_MODEL.md`      | Authority, prediction, persistence, replication validation diagnostics                 |
| `AUTHORING_PERSONAS.md`             | Profile-layered diagnostic presentation                                                |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md` | Build/package/publish diagnostic gates                                                 |
| `DependencyGraph.md`                | Boundary violations and dependency diagnostics                                         |

---

## 3. Ownership Rule

Phase 0 migration audit:

* [x] Record the current SagaDiagnostics and Public/Private migration reality.

  Done means the repository has a documentation-first audit before new
  diagnostics implementation work:

  ```txt
  docs/architecture/DIAGNOSTICS_MIGRATION_AUDIT.md
  ```

  The audit records that the repository currently uses a mixed layout:
  `Engine/Public` and `Engine/Private` for Engine, plus module-local
  `include/src` roots for Runtime, Server, Shared, Collaboration, Backends, and
  Tools. It also records that a root
  `Source/SagaEngine/<Module>/Public/Private` migration is not an immediate
  compatibility step because current architecture tests and CMake layout do not
  use that tree.

  The next SagaDiagnostics implementation slices should preserve these
  dependency rules:

  ```txt
  Core must not depend on Diagnostics.
  Diagnostics may depend on Core.
  Runtime, Server, Net, and Tools may depend on Diagnostics where appropriate.
  Diagnostics must not hard-depend on SDE.
  ```

  This is an audit milestone, not a production-readiness claim. It does not add
  logging, health, lifetime, memory, crash, stress, chaos, validation, or fault
  systems.

Phase 1 foundation consolidation:

* [x] Consolidate the first buildable SagaDiagnostics foundation slice.

  Done means existing Engine-shaped diagnostics code has a coherent minimal
  foundation around config, explicit diagnostics ownership, structured logging,
  health metrics, lifetime records, deterministic tests, and a focused test
  target:

  ```txt
  Engine/Public/SagaEngine/Diagnostics/
  Engine/Private/SagaEngine/Diagnostics/
  Engine/Public/SagaEngine/Core/Log/
  Tests/Unit/Diagnostics/DiagnosticFoundationTests.cpp
  cmake/modules/SagaTests.cmake
  ```

  Phase 1 preserves the dependency rule:

  ```txt
  Core must not depend on Diagnostics.
  Diagnostics may depend on Core/CoreLog.
  Diagnostics must not depend on Server, Runtime, Editor, Product, Tools, or SDE.
  ```

  Phase 1 completed only foundation consolidation. These remain non-claims:
  no server integration, no crash reports, no memory/resource tracking, no
  StressArena, no NetworkChaos, no StateValidation, no FaultBoundary, no
  production readiness, and no full raw CTest health claim unless full raw CTest
  is separately run and passes.

Phase 2 operational reports and ZoneServer observability:

* [x] Add the first operational diagnostics vertical slice.

  Done means `SagaDiagnostics` now owns a deterministic Engine report model and
  writer plus optional server-side emission through a Server-owned dependency:

  ```txt
  Engine/Public/SagaEngine/Diagnostics/Report/DiagnosticReport.hpp
  Engine/Public/SagaEngine/Diagnostics/Report/DiagnosticReportWriter.hpp
  Engine/Private/SagaEngine/Diagnostics/Report/DiagnosticReportWriter.cpp
  Tests/Unit/Diagnostics/DiagnosticReportTests.cpp
  Tests/Unit/Server/ZoneServerDiagnosticsTests.cpp
  ```

  Report kinds added in this phase are `health_snapshot`, `lifetime_leaks`, and
  `operational_snapshot`. Reports carry summary counts, health metrics, alive
  lifetime leak records, recent log events, deterministic generation sequence
  numbers, and sorted string metadata.

  `ZoneServer` now accepts optional `SagaDiagnostics` injection and emits first
  local metrics for `server.tick.count`, `world.entities.active`,
  `server.movement.accepted_inputs`, `server.movement.rejected_inputs`, and
  `server.packets.rejected` only when diagnostics is attached.

  Phase 2 preserves the dependency rule:

  ```txt
  Core must not depend on Diagnostics.
  SagaDiagnostics may depend on Core/CoreLog and privately on nlohmann_json.
  Server may privately depend on SagaDiagnostics.
  SagaDiagnostics must not depend on Server, Runtime, Editor, Product, Tools, or SDE.
  ```

  Phase 2 non-claims: no crash reports, no MemoryTracker/ResourceTracker, no
  StressArena, no NetworkChaos, no StateValidation, no FaultBoundary, no
  production readiness, and no full raw CTest health claim unless full raw CTest
  is separately run and passes.

Phase 3 crash context reports and reliability rules:

* [x] Add the first crash-context and reliability diagnostics layer.

  Done means `SagaDiagnostics` now owns manual crash-context and reliability
  report contracts, health rule evaluation, and deterministic lifetime leak
  diagnostic summaries:

  ```txt
  Engine/Public/SagaEngine/Diagnostics/Crash/CrashContext.hpp
  Engine/Public/SagaEngine/Diagnostics/Crash/CrashReport.hpp
  Engine/Public/SagaEngine/Diagnostics/Crash/CrashReportBuilder.hpp
  Engine/Public/SagaEngine/Diagnostics/Health/HealthRule.hpp
  Engine/Public/SagaEngine/Diagnostics/Health/HealthRuleResult.hpp
  Engine/Public/SagaEngine/Diagnostics/Health/HealthSeverity.hpp
  Engine/Public/SagaEngine/Diagnostics/Lifetime/LifetimeLeakDiagnostic.hpp
  Tests/Unit/Diagnostics/DiagnosticReliabilityTests.cpp
  ```

  Report kinds added in this phase are `manual_crash_report`,
  `fatal_error_report`, `reliability_failure_report`, and `slow_tick_report`.
  CrashReport output includes current health metrics, health rule violations,
  lifetime leak candidates, lifetime leak summary counts by type and owner
  system, recent log events, deterministic generation sequence numbers, and
  sorted context metadata.

  HealthRule support added in this phase covers `HealthSeverity` values
  `info`, `warning`, `error`, and `critical`, plus gauge greater-than, gauge
  less-than, counter greater-than, and timing greater-than checks. Missing
  metrics are deterministic not-evaluated results.

  `ZoneServer` keeps diagnostics optional and adds reliability metrics for
  `server.tick.ms` and `server.tick.budget_overruns` when actual tick duration
  data is available. Diagnostics still emit metrics/logs only and do not mutate
  simulation state.

  Phase 3 preserves the dependency rule:

  ```txt
  Core must not depend on Diagnostics.
  SagaDiagnostics may depend on Core/CoreLog and privately on nlohmann_json.
  Server may privately depend on SagaDiagnostics.
  SagaDiagnostics must not depend on Server, Runtime, Editor, Product, Tools, or SDE.
  ```

  Phase 3 non-claims: no production crash safety, no unsafe OS signal/SEH crash
  handlers, no stack trace support requirement, no MemoryTracker/ResourceTracker,
  no StressArena, no NetworkChaos, no StateValidation, no FaultBoundary, no
  production readiness, and no full raw CTest health claim unless full raw CTest
  is separately run and passes.

Phase 4 safe SagaStressArena diagnostics proof:

* [x] Add the first bounded stress/soak diagnostics artifact proof.

  Done means the repository now has a tool-level `SagaStressArena` direct local
  ZoneServer harness for deterministic diagnostics artifact generation:

  ```txt
  Tools/SagaStressArena/include/SagaStressArena/
  Tools/SagaStressArena/src/
  Tools/SagaStressArena/cli/main.cpp
  Tests/Unit/Tools/SagaStressArenaTests.cpp
  docs/architecture/DIAGNOSTICS_STRESS_ARENA_V1.md
  ```

  The supported v1 scenario is `direct_zone_diagnostics_smoke`. It uses the
  existing local ZoneServer harness path rather than real network bot transport.
  It writes `direct_zone_stress_operational_snapshot.json`,
  `direct_zone_stress_reliability_report.json`, and
  `direct_zone_stress_lifetime_leaks.json`.

  Stress tiers are explicit and bounded: `smoke` is the default safe tier,
  `mini_soak` is bounded and manual/local, and `heavy` is documented as
  manual opt-in only in v1. Focused tests and default CLI behavior do not
  execute heavy stress.

  Phase 4 preserves the dependency rule:

  ```txt
  SagaStressArenaLib may depend on SagaServerLib and SagaDiagnostics.
  SagaDiagnostics must not depend on Tools or SagaStressArenaLib.
  SagaServerLib must not depend on Tools or SagaStressArenaLib.
  ```

  Phase 4 non-claims: no full network stress, no real bot swarm, no
  production-grade load testing, no MMO-scale load claim, no 100/500/1000 client
  claim, no default heavy stress, no MemoryTracker/ResourceTracker, no
  NetworkChaos, no StateValidation, no FaultBoundary, no production readiness,
  and no full raw CTest health claim unless full raw CTest is separately run
  and passes.

Phase 5 memory/resource snapshot foundation:

* [x] Add the first explicit memory/resource diagnostics snapshot foundation.

  Done means `SagaDiagnostics` now owns deterministic, explicit memory and
  resource trackers:

  ```txt
  Engine/Public/SagaEngine/Diagnostics/Memory/
  Engine/Public/SagaEngine/Diagnostics/Resource/
  Engine/Private/SagaEngine/Diagnostics/Memory/
  Engine/Private/SagaEngine/Diagnostics/Resource/
  Tests/Unit/Diagnostics/DiagnosticMemoryResourceTests.cpp
  docs/architecture/DIAGNOSTICS_MEMORY_RESOURCE_SNAPSHOT_FOUNDATION.md
  ```

  `MemoryTracker` records explicit per-system counters and snapshots current
  bytes, peak bytes, total allocated bytes, total freed bytes, and active
  explicit allocation count. `ResourceTracker` records explicitly registered
  socket, file, timer, job, asset handle, database connection, thread, and other
  resource records. Active resources are leak candidates and summaries are
  deterministic by type and owner system.

  Operational reports and crash/reliability reports now include
  `memoryRecords`, `resourceSummary`, `activeResources`, and summary counts for
  memory systems and active resources. `SagaStressArena` records deterministic
  fake/explicit memory and resource records in its direct local ZoneServer
  report artifacts.

  Phase 5 preserves the dependency rule:

  ```txt
  Core must not depend on Diagnostics.
  SagaDiagnostics may depend on Core/CoreLog and privately on nlohmann_json.
  Server and Tools may depend on SagaDiagnostics at their owning boundaries.
  SagaDiagnostics must not depend on Server, Runtime, Editor, Product, Tools, or SDE.
  ```

  Phase 5 non-claims: no custom allocator, no operator new/delete override, no
  allocator replacement, no raw allocation interception, no stack traces per
  allocation, no OS memory polling, no OS handle enumeration, no real
  socket/file scanning, no full leak detection, no production memory safety, no
  NetworkChaos, no StateValidation, no FaultBoundary, no production readiness,
  and no full raw CTest health claim unless full raw CTest is separately run and
  passes.

* [ ] Keep diagnostic contracts in `SagaShared` where genuinely shared.

  Done means common diagnostic data types can live in:

  ```txt
  Shared/include/SagaShared/Diagnostics/
  ```

  But implementations remain owned by their subsystems.

* [ ] Keep diagnostic emission owned by each subsystem.

  Done means:

  * SDE emits SDE diagnostics,
  * Forge emits build/package/publish diagnostics,
  * Prism emits analysis diagnostics,
  * AssetPipeline emits asset diagnostics,
  * Scripting toolchain emits script diagnostics,
  * Runtime emits runtime diagnostics,
  * Server emits server diagnostics,
  * SagaCollaboration emits collaboration diagnostics,
  * SagaEditor emits editor diagnostics,
  * Saga product shell presents product-level summaries.

* [ ] Keep diagnostic presentation separate from diagnostic truth.

  Done means:

  * editor Problems panel is presentation,
  * Saga dashboard is presentation,
  * CLI output is presentation,
  * JSON report is structured truth/report,
  * authoring profile can simplify wording but cannot hide publish-blocking truth.

---

## 4. Core Diagnostic Model

* [ ] Add shared diagnostic payload model.

  Done means a diagnostic can describe:

  * diagnostic id,
  * diagnostic code,
  * category,
  * source system,
  * severity,
  * title,
  * message,
  * technical details,
  * resource references,
  * source locations,
  * artifact references,
  * package references,
  * build step references,
  * graph references,
  * script references,
  * asset references,
  * collaboration references,
  * suggested action,
  * raw metadata payload,
  * timestamp where applicable.

Expected files:

```txt
Shared/include/SagaShared/Diagnostics/DiagnosticId.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticCode.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticSeverity.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticCategory.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticSource.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticLocation.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticPayload.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticSummary.hpp
```

  `0.0.8-dev.6` shipped the foundational data-only payload contracts:

  ```txt
  Shared/include/SagaShared/Diagnostics/DiagnosticCode.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticSeverity.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticCategory.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticSource.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticLocation.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticPayload.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticSummary.hpp
  Tests/Unit/Shared/DiagnosticContractsTests.cpp
  ```

  The full core model remains open for diagnostic ids, report envelopes, timestamp policy, and subsystem emitters.

* [x] Keep diagnostic payload serializable.

  Done means diagnostics can be stored in reports and consumed by editor/product/CI without linking subsystem internals.

* [x] Keep raw subsystem data optional.

  Done means diagnostics may include raw metadata, but consumers can still understand the diagnostic without private implementation headers.

---

## 5. Severity Model

Required severities:

```txt
Trace
Info
Warning
Error
Fatal
BuildBlocking
PublishBlocking
Security
```

* [x] Define severity semantics.

  Done means:

  * `Trace` is detailed low-level information,
  * `Info` is non-problem informational status,
  * `Warning` means action may be needed,
  * `Error` means the operation/resource is invalid,
  * `Fatal` means the subsystem/session cannot continue safely,
  * `BuildBlocking` means build/profile cannot continue,
  * `PublishBlocking` means publish/release cannot proceed,
  * `Security` means trust boundary or abuse-related issue.

* [ ] Allow profile-specific presentation, not profile-specific truth.

  Done means Beginner mode may show simpler wording, but `PublishBlocking` remains `PublishBlocking`.

* [x] Define severity aggregation.

  Done means reports can summarize highest severity and counts per category/source system.

  Represented by `DiagnosticSummary` count fields, `DiagnosticSummary::Add`, and `DiagnosticSeverityRank`.

---

## 6. Diagnostic Categories

Required top-level category families:

```txt
Project.*
Workspace.*
Editor.*
Profile.*
Graph.*
Authority.*
Script.*
Asset.*
SDE.*
Build.*
Package.*
Publish.*
Prism.*
Collaboration.*
Runtime.*
Server.*
Network.*
Replication.*
Prediction.*
Persistence.*
Security.*
Toolchain.*
DependencyBoundary.*
Crash.*
Shutdown.*
Performance.*
Memory.*
```

* [ ] Maintain category registry.

  Done means category prefixes are documented and collisions are avoided.

* [ ] Use stable diagnostic codes.

  Done means codes are stable enough for CI filters, tests, reports, and documentation.

Examples:

```txt
Graph.Node.MissingInput
Authority.ClientOnlyServerWrite
Script.Binding.SignatureMismatch
Asset.Cook.StaleArtifact
SDE.Semantic.UnknownType
Build.Step.MissingOutput
Package.Manifest.HashMismatch
Publish.Blocked.CollaborationConflict
Prism.Boundary.ForbiddenInclude
Runtime.Package.UnsupportedVersion
Server.Request.UnauthorizedAction
```

---

## 7. Resource and Source Location Model

* [ ] Add diagnostic locations.

  Done means diagnostics can point to:

  * source file path,
  * line/column range,
  * SDE source range,
  * C# source range,
  * C++ source range,
  * graph node/pin/edge,
  * asset metadata/source,
  * package manifest entry,
  * build step,
  * runtime entity where safe,
  * server session/connection where safe,
  * collaboration conflict/lock/claim.

Expected files:

```txt
Shared/include/SagaShared/Resources/ResourceRef.hpp
Shared/include/SagaShared/Resources/SourceLocation.hpp
Shared/include/SagaShared/Resources/SourceRange.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticLocation.hpp
```

* [ ] Add source-map-aware diagnostics.

  Done means diagnostics from generated artifacts can navigate back to:

  * SDE source,
  * graph node/pin/edge,
  * C# source,
  * generated code origin,
  * asset source/import settings.

* [ ] Add redaction policy for sensitive runtime/server locations.

  Done means server diagnostics do not expose secrets, tokens, private user data, or backend credentials in reports.

---

## 8. Diagnostic Reports

Required report families:

```txt
sde_diagnostics.json
build_report.json
diagnostics_report.json
publish_report.json
prism_boundary_report.json
prism_stale_report.json
asset_diagnostics.json
script_diagnostics.json
runtime_diagnostics.json
server_diagnostics.json
collaboration_diagnostics.json
crash_report.json
shutdown_report.json
health_snapshot.json
```

* [ ] Define report envelope.

  Done means every report has:

  * report type,
  * report version,
  * producer tool/system,
  * producer version,
  * project id/root where applicable,
  * workspace id where applicable,
  * profile where applicable,
  * generated timestamp where useful,
  * input summary,
  * diagnostics list,
  * summary counts,
  * machine-readable metadata.

Expected files:

```txt
Shared/include/SagaShared/Diagnostics/DiagnosticReport.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticReportEnvelope.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticReportSummary.hpp
```

* [ ] Keep report schemas versioned.

  Done means old/new report versions can be rejected or migrated clearly.

* [ ] Emit both human-readable and machine-readable output where useful.

  Done means CLI tools can print concise summaries while writing JSON reports.

---

## 9. SDE Diagnostics

* [ ] Stabilize SDE diagnostics.

  Required diagnostic families:

```txt
SDE.Lexer.*
SDE.Parser.*
SDE.Semantic.*
SDE.Schema.*
SDE.Type.*
SDE.Graph.*
SDE.Artifact.*
SDE.Dependency.*
SDE.SourceMap.*
```

* [ ] Require SDE source ranges.

  Done means parse/semantic/graph diagnostics point to exact source ranges.

* [ ] Emit machine-readable SDE diagnostics.

  Done means SDE can emit:

```txt
sde_diagnostics.json
sde_artifact_manifest.json
sde_dependency_manifest.json
sde_source_map.json
```

* [ ] Keep SDE diagnostics standalone.

  Done means SDE diagnostics do not require Saga modules or SagaShared if SDE is being used externally. Adapters may translate to SagaShared-compatible payloads outside SDE core.

---

## 10. Graph Diagnostics

* [ ] Add graph diagnostics model.

  Required diagnostic families:

```txt
Graph.Node.*
Graph.Pin.*
Graph.Edge.*
Graph.Block.*
Graph.Macro.*
Graph.Artifact.*
Graph.GeneratedCode.*
Graph.Package.*
```

* [ ] Support graph location references.

  Done means diagnostics can reference:

  * graph id,
  * node id,
  * pin id,
  * edge id,
  * block id,
  * macro/subgraph id,
  * source map ref.

* [ ] Add editor navigation.

  Done means clicking graph diagnostics opens graph editor and selects affected node/pin/edge.

Examples:

```txt
Graph.Edge.TypeMismatch
Graph.Block.UnknownBlockId
Graph.Artifact.Stale
Graph.GeneratedCode.SourceMapMissing
```

---

## 11. Authority Diagnostics

* [ ] Add authority diagnostics model.

  Required diagnostic families:

```txt
Authority.ExecutionDomain.*
Authority.SideEffect.*
Authority.Persistence.*
Authority.Replication.*
Authority.Prediction.*
Authority.Security.*
Authority.Package.*
```

* [ ] Support authority metadata references.

  Done means diagnostics can identify:

  * required authority,
  * actual authority,
  * execution domain,
  * side effect,
  * persistence effect,
  * replication effect,
  * prediction safety,
  * package destination.

Examples:

```txt
Authority.ClientOnlyServerWrite
Authority.PersistentWriteWithoutServerAuthority
Authority.PredictionUnsafeBlockInPredictedGraph
Authority.ServerOnlyArtifactInClientPackage
```

* [ ] Layer presentation by authoring profile.

  Beginner example:

```txt
This reward must run on the server. Saga can create a safe server request flow.
```

Technical example:

```txt
Authority.ClientOnlyServerWrite
Graph: HUD.OnQuestCompleteButton
Node: Inventory.GiveItem
RequiredAuthority: ServerOnly
ActualAuthority: ClientOnly
SideEffects: WriteAuthoritativeState, WritePersistentState
```

Same truth. Different presentation.

---

## 12. Script Diagnostics

* [ ] Add script diagnostics model.

  Required diagnostic families:

```txt
Script.Project.*
Script.Source.*
Script.Compile.*
Script.Binding.*
Script.GeneratedCode.*
Script.Authority.*
Script.Package.*
Script.HotReload.*
```

* [ ] Support C# source ranges.

  Done means diagnostics can point to file/line/column and symbols where available.

* [ ] Support block binding diagnostics.

  Required examples:

```txt
Script.Binding.MissingBlockCallableMetadata
Script.Binding.SignatureMismatch
Script.Binding.UnsupportedParameterType
Script.Binding.AuthorityMetadataMissing
Script.Binding.ManifestStale
```

* [ ] Support generated code origin diagnostics.

  Done means diagnostics can connect generated C# back to graph/SDE source.

---

## 13. Asset Diagnostics

* [ ] Add asset diagnostics model.

  Required diagnostic families:

```txt
Asset.Source.*
Asset.Import.*
Asset.Metadata.*
Asset.Dependency.*
Asset.Cook.*
Asset.Manifest.*
Asset.Package.*
Asset.Runtime.*
Asset.Streaming.*
Asset.Budget.*
```

Required asset diagnostic fields:

```txt
assetId
assetKind
sourcePath
artifactId
artifactPath
importerId
cookerId
cookProfile
targetPlatform
sourceHash
artifactHash
expectedHash
actualHash
dependencyRef
suggestedFix
```

Examples:

```txt
Asset.Import.UnsupportedFormat
Asset.Import.MissingDependency
Asset.Cook.StaleArtifact
Asset.Manifest.HashMismatch
Asset.Runtime.MissingArtifact
Asset.Budget.TextureMemoryExceeded
```

* [ ] Support content browser and asset inspector navigation.

  Done means asset diagnostics open the relevant asset, source file, metadata, import settings, or cooked artifact info.

---

## 14. Build Diagnostics

* [ ] Add build diagnostics model.

  Required diagnostic families:

```txt
Build.Profile.*
Build.Step.*
Build.Toolchain.*
Build.Cache.*
Build.Input.*
Build.Output.*
Build.Report.*
```

Required build diagnostic fields:

```txt
buildId
profile
stepId
stepKind
toolName
toolVersion
inputRef
outputRef
exitCode
reportPath
suggestedAction
```

Examples:

```txt
Build.Step.MissingInput
Build.Step.MissingOutput
Build.Toolchain.VersionMismatch
Build.Cache.StaleHitRejected
Build.Profile.UnsupportedTarget
```

* [ ] Integrate with Forge reports.

  Done means Forge emits build diagnostics into:

```txt
build_report.json
diagnostics_report.json
```

---

## 15. Package Diagnostics

* [ ] Add package diagnostics model.

  Required diagnostic families:

```txt
Package.Manifest.*
Package.Artifact.*
Package.Client.*
Package.Server.*
Package.Profile.*
Package.Hash.*
Package.Compatibility.*
```

Examples:

```txt
Package.Manifest.MissingArtifact
Package.Manifest.HashMismatch
Package.Client.ServerOnlyArtifact
Package.Server.MissingAuthoritativeGraph
Package.Compatibility.UnsupportedRuntimeVersion
```

* [ ] Support package manifest navigation.

  Done means diagnostics can point to package manifest entries and staged artifacts.

---

## 16. Publish Diagnostics

* [ ] Add publish diagnostics model.

  Required diagnostic families:

```txt
Publish.Readiness.*
Publish.Blocker.*
Publish.Permission.*
Publish.Collaboration.*
Publish.Package.*
Publish.Profile.*
```

* [ ] Add publish blocker diagnostics.

  Required blocker kinds:

```txt
ProjectManifestInvalid
SDECompileError
GraphValidationError
AuthorityValidationError
ScriptCompileError
AssetCookError
StaleArtifact
PackageManifestInvalid
CollaborationConflict
PermissionDenied
ToolchainInvalid
RuntimeManifestInvalid
ServerPackageInvalid
DiagnosticsFatal
```

* [ ] Emit publish report.

  Required output:

```txt
publish_report.json
```

* [ ] Keep publish diagnostics exact.

  Bad:

```txt
Publish failed.
```

Good:

```txt
Publish blocked: ServerOnly graph artifact QuestReward is staged in client package.
```

---

## 17. Prism Diagnostics

* [ ] Add Prism diagnostic model.

  Required diagnostic families:

```txt
Prism.Index.*
Prism.Dependency.*
Prism.Boundary.*
Prism.Stale.*
Prism.GeneratedOrigin.*
Prism.Artifact.*
Prism.Package.*
```

Examples:

```txt
Prism.Boundary.ForbiddenInclude
Prism.Stale.GeneratedCode
Prism.Stale.CookedArtifact
Prism.GeneratedOrigin.MissingSourceMap
Prism.Package.UnlistedArtifactPresent
```

* [ ] Emit Prism reports.

  Required reports:

```txt
boundary_report.json
stale_report.json
generated_origin_report.json
artifact_relationship_report.json
package_relationship_report.json
```

* [ ] Make Prism diagnostics source/resource linked.

  Done means diagnostics point to source file, include edge, generated file, artifact, package manifest, or originating source.

---

## 18. Collaboration Diagnostics

* [ ] Add collaboration diagnostics model.

  Required diagnostic families:

```txt
Collaboration.Session.*
Collaboration.Permission.*
Collaboration.Presence.*
Collaboration.Claim.*
Collaboration.Lock.*
Collaboration.Conflict.*
Collaboration.ChangeStream.*
Collaboration.Backend.*
Collaboration.PublishGate.*
Collaboration.Offline.*
```

Examples:

```txt
Collaboration.Permission.PublishDenied
Collaboration.Lock.ResourceLocked
Collaboration.Conflict.GraphNodeConflict
Collaboration.PublishGate.UnresolvedConflict
Collaboration.Offline.UnverifiedTeamState
```

* [ ] Connect collaboration diagnostics to resources.

  Done means diagnostics reference exact graph/script/asset/SDE/package resources.

* [ ] Feed collaboration blockers into publish report.

  Done means unresolved blocking conflicts and critical locks can become publish blockers.

---

## 19. Runtime Diagnostics

* [ ] Add runtime diagnostics model.

  Required diagnostic families:

```txt
Runtime.Package.*
Runtime.Artifact.*
Runtime.Asset.*
Runtime.Graph.*
Runtime.Script.*
Runtime.Authority.*
Runtime.Frame.*
Runtime.Memory.*
Runtime.Shutdown.*
Runtime.Crash.*
```

Examples:

```txt
Runtime.Package.UnsupportedVersion
Runtime.Artifact.HashMismatch
Runtime.Asset.MissingArtifact
Runtime.Graph.InvalidExecutionDomain
Runtime.Script.BindingMissing
Runtime.Memory.BudgetExceeded
```

* [ ] Add runtime diagnostic sink.

  Done means runtime can emit diagnostics to:

  * in-memory collector,
  * log output,
  * JSON report,
  * product/editor preview channel where applicable.

Expected files:

```txt
Engine/include/SagaEngine/Diagnostics/RuntimeDiagnostic.hpp
Engine/include/SagaEngine/Diagnostics/RuntimeDiagnosticSink.hpp
Engine/src/SagaEngine/Diagnostics/RuntimeDiagnostic.cpp
```

* [ ] Add runtime health snapshots.

  Done means runtime can report:

  * frame timing,
  * memory pressure,
  * asset streaming pressure,
  * loaded package ids,
  * artifact load failures,
  * diagnostic counts.

---

## 20. Server Diagnostics

* [ ] Add server diagnostics model.

  Required diagnostic families:

```txt
Server.Package.*
Server.Session.*
Server.Network.*
Server.Request.*
Server.Authority.*
Server.Replication.*
Server.Persistence.*
Server.Security.*
Server.Tick.*
Server.Shard.*
Server.Zone.*
Server.Shutdown.*
Server.Crash.*
```

Examples:

```txt
Server.Request.UnauthorizedAction
Server.Authority.ClientAttemptedPersistentWrite
Server.Replication.SnapshotBudgetExceeded
Server.Persistence.TransactionRollback
Server.Security.RateLimitExceeded
Server.Tick.SlowTick
```

* [ ] Add server health snapshots.

  Done means server can report:

  * tick duration,
  * connected clients,
  * packet rates,
  * invalid packet count,
  * replication bandwidth,
  * graph/script invocation count,
  * persistence latency,
  * shard/zone load,
  * memory usage.

* [ ] Add long-running server degradation diagnostics.

  Done means server can identify:

  * rising memory usage,
  * stale sessions,
  * growing queues,
  * slow persistence calls,
  * replication backlog,
  * tick spikes.

---

## 21. Network, Replication, Prediction Diagnostics

* [ ] Add network diagnostics.

  Required diagnostics:

```txt
Network.Packet.Malformed
Network.Packet.Oversized
Network.Packet.UnknownType
Network.Connection.Timeout
Network.Connection.ProtocolMismatch
Network.RateLimit.Exceeded
```

* [ ] Add replication diagnostics.

  Required diagnostics:

```txt
Replication.Snapshot.Stale
Replication.Snapshot.MissingBaseline
Replication.Snapshot.InvalidDelta
Replication.Apply.InvalidComponentPayload
Replication.Budget.Exceeded
Replication.State.InvalidTransition
```

* [ ] Add prediction/reconciliation diagnostics.

  Required diagnostics:

```txt
Prediction.Input.MissingAck
Prediction.Replay.Failed
Prediction.Error.TooLarge
Prediction.Context.UnsafeArtifact
Reconciliation.Correction.Applied
Reconciliation.Buffer.Overflow
```

---

## 22. Crash and Shutdown Diagnostics

* [ ] Add crash report model.

  Done means crash reports can include:

  * process id,
  * subsystem,
  * build id,
  * package id,
  * runtime/server profile,
  * last diagnostics,
  * loaded artifacts,
  * active graph/script refs where safe,
  * stack trace where available,
  * redacted environment info.

* [ ] Add shutdown report model.

  Done means shutdown reports can include:

  * shutdown reason,
  * active sessions,
  * pending saves/transactions,
  * pending network disconnects,
  * pending asset loads,
  * diagnostics summary.

* [ ] Add safe redaction.

  Done means crash/shutdown reports do not leak credentials or private tokens.

---

## 23. Performance and Memory Diagnostics

* [ ] Add performance diagnostic events.

  Required metrics:

```txt
frame time
tick time
build step time
asset cook time
SDE compile time
script compile time
Prism analysis time
package staging time
network send/receive rate
replication bandwidth
```

* [ ] Add memory diagnostic events.

  Required metrics:

```txt
runtime memory usage
server memory usage
asset residency memory
GPU asset estimates where available
package size
streaming group budget
cook output size
```

* [ ] Add budget diagnostics.

  Done means diagnostics can warn/block when:

  * asset memory exceeds budget,
  * package size exceeds budget,
  * streaming group exceeds budget,
  * replication bandwidth exceeds budget,
  * server tick exceeds threshold.

---

## 24. Editor Diagnostics UX

* [ ] Add Problems panel integration.

  Done means editor Problems panel can show diagnostics from:

  * editor systems,
  * SDE,
  * graph validation,
  * authority validation,
  * scripting,
  * assets,
  * Forge build/package/publish,
  * Prism,
  * collaboration,
  * runtime preview,
  * server preview.

* [ ] Add Diagnostics Report panel.

  Done means technical users can inspect raw reports:

  * build reports,
  * Prism reports,
  * SDE reports,
  * asset reports,
  * script reports,
  * runtime/server reports.

* [ ] Add diagnostic navigation.

  Done means clicking diagnostics opens/selects:

  * SDE source range,
  * graph node/pin/edge,
  * C# source range,
  * C++ source range,
  * asset source/metadata,
  * package manifest entry,
  * build step/report,
  * collaboration conflict/lock,
  * runtime/server report section.

* [ ] Add profile-layered presentation.

  Done means diagnostic detail levels include:

```txt
Simple
Standard
Technical
RawPayload
```

---

## 25. Saga Product Diagnostics UX

* [ ] Add product-level diagnostics summary.

  Done means Saga dashboard/build/publish views show:

  * project health,
  * build health,
  * publish readiness,
  * unresolved blockers,
  * collaboration health,
  * runtime/server preview health,
  * toolchain health.

* [ ] Add exact publish blockers.

  Done means product publish UI can explain:

```txt
Publish blocked:
- 1 authority error
- 2 script compile errors
- 1 stale cooked asset
- 1 collaboration conflict
```

* [ ] Keep product diagnostics as presentation/aggregation.

  Done means Saga does not own every subsystem diagnostic implementation.

---

## 26. CLI and CI Diagnostics

* [ ] Require `--json` output from major tools.

  Tools:

```txt
sde
forge
prism
asset-pipeline
script-compiler
host where useful
```

* [ ] Require stable exit codes.

  Required categories:

```txt
Success
ValidationFailure
CompileFailure
AnalysisFailure
StaleArtifact
BoundaryViolation
PackageFailure
PublishBlocked
ToolchainFailure
RuntimeStartupFailure
ServerStartupFailure
InternalError
UserCancelled
```

* [ ] Emit CI artifacts.

  Required artifacts:

```txt
build_report.json
diagnostics_report.json
publish_report.json
boundary_report.json
stale_report.json
sde_diagnostics.json
asset_diagnostics.json
script_diagnostics.json
runtime_diagnostics.json
server_diagnostics.json
```

---

## 27. Diagnostic Storage and Retention

* [ ] Define local diagnostic storage policy.

  Done means reports are stored under predictable project paths:

```txt
<Project>/Build/Reports/
<Project>/Build/Diagnostics/
<Project>/Build/Manifests/
```

* [ ] Define report retention policy.

  Done means users can configure:

  * keep latest only,
  * keep last N reports,
  * keep reports for CI artifacts,
  * clean reports with build clean command.

* [ ] Keep report storage separate from source truth.

  Done means generated diagnostics do not mutate source/project manifests unless explicitly intended.

---

## 28. Privacy and Redaction

* [ ] Add redaction model.

  Done means diagnostics can mark fields as:

```txt
Public
ProjectLocal
PrivateUser
Secret
```

* [ ] Redact secrets in reports.

  Must redact:

  * auth tokens,
  * backend credentials,
  * secret environment variables,
  * private connection strings,
  * personal account identifiers where not needed.

* [ ] Support safe report sharing.

  Done means user can export diagnostic reports without accidentally leaking secrets.

---

## 29. Testing Strategy

### 29.1 Contract Tests

* [ ] Add diagnostic contract tests.

  Required coverage:

  * diagnostic id serialization,
  * severity serialization,
  * category serialization,
  * location serialization,
  * resource refs,
  * report envelope versioning,
  * raw metadata payload handling.

---

### 29.2 Subsystem Emission Tests

* [ ] Add emission tests for each subsystem.

  Required coverage:

  * SDE emits source-ranged diagnostics,
  * graph emits node/pin/edge diagnostics,
  * authority emits metadata diagnostics,
  * scripting emits source/binding diagnostics,
  * asset pipeline emits import/cook diagnostics,
  * Forge emits build/publish diagnostics,
  * Prism emits stale/boundary diagnostics,
  * collaboration emits lock/conflict diagnostics,
  * runtime/server emit package/artifact/network diagnostics.

---

### 29.3 Navigation Tests

* [ ] Add editor navigation tests.

  Required coverage:

  * SDE diagnostic opens source range,
  * graph diagnostic selects node,
  * script diagnostic opens C# range,
  * asset diagnostic opens asset inspector,
  * package diagnostic opens manifest entry,
  * collaboration diagnostic opens conflict panel,
  * build diagnostic opens build report step.

---

### 29.4 Report Tests

* [ ] Add report schema tests.

  Required coverage:

  * build report valid,
  * publish report valid,
  * Prism report valid,
  * SDE report valid,
  * runtime/server report valid,
  * missing required fields rejected,
  * unsupported report version rejected.

---

### 29.5 Redaction Tests

* [ ] Add redaction tests.

  Required coverage:

  * tokens redacted,
  * connection strings redacted,
  * raw environment secrets redacted,
  * safe fields preserved,
  * exported report safe mode.

---

## 30. MVP Vertical Slice

The first diagnostics vertical slice should cover a single broken MMO Starter project.

Required project failures:

```txt
1. QuestReward graph has client-side server write.
2. QuestXp.cs has invalid block-callable signature.
3. terrain_albedo.png cooked artifact is stale.
4. ServerOnly graph artifact appears in client package manifest.
5. User lacks PublishRelease permission.
```

Required behavior:

1. SDE emits graph/source diagnostic where applicable.
2. Authority validation emits `Authority.ClientOnlyServerWrite`.
3. Script compiler emits `Script.Binding.SignatureMismatch`.
4. AssetPipeline/Prism emits `Asset.Cook.StaleArtifact`.
5. Forge emits `Package.Client.ServerOnlyArtifact`.
6. Collaboration emits `Collaboration.Permission.PublishDenied`.
7. Forge publish-check emits `publish_report.json` with blockers.
8. Saga product view shows exact publish blockers.
9. Editor Problems panel shows all diagnostics with profile-layered messages.
10. Clicking each diagnostic navigates to graph node, C# source, asset metadata, package manifest entry, or permission/collaboration panel.

This slice proves diagnostics are not scattered strings.

They are navigable, structured evidence.

---

## 31. Non-Goals

Diagnostics must not become:

* the product shell,
* editor UI implementation,
* runtime/server implementation,
* SDE compiler implementation,
* Forge build planner,
* Prism analyzer,
* asset cooker,
* script compiler,
* collaboration conflict engine,
* logging-only system,
* terminal-output scraping layer,
* place where subsystem ownership disappears.

Diagnostics describe and transport evidence.

They do not own the systems that produce the evidence.

---

## 32. Risk Register

### 32.1 Risk: Diagnostics Become Plain Logs

Mitigation:

* structured payloads,
* stable codes,
* source/resource refs,
* reports,
* editor navigation tests.

---

### 32.2 Risk: Diagnostics Become Too Centralized

Mitigation:

* contracts shared,
* emission owned by subsystem,
* presentation owned by product/editor/CLI,
* no mega diagnostic implementation owner.

---

### 32.3 Risk: Beginner Mode Hides Critical Truth

Mitigation:

* presentation can simplify,
* severity/truth unchanged,
* publish-blocking diagnostics always visible.

---

### 32.4 Risk: Reports Leak Secrets

Mitigation:

* redaction model,
* report safe export,
* redaction tests,
* avoid raw environment dumps by default.

---

### 32.5 Risk: Diagnostics Cannot Navigate to Real Source

Mitigation:

* source maps,
* resource refs,
* graph node/pin/edge refs,
* package manifest entry refs,
* editor navigation tests.

---

## 33. Suggested File Targets

Expected shared files:

```txt
Shared/include/SagaShared/Diagnostics/DiagnosticId.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticCode.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticSeverity.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticCategory.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticSource.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticLocation.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticPayload.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticReport.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticReportEnvelope.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticReportSummary.hpp
```

Expected editor files:

```txt
Editor/include/SagaEditor/Diagnostics/EditorDiagnostic.h
Editor/include/SagaEditor/Diagnostics/IEditorDiagnosticsService.h
Editor/include/SagaEditor/Diagnostics/DiagnosticNavigation.h
Editor/include/SagaEditor/Panels/ProblemsPanel.h
Editor/include/SagaEditor/Panels/DiagnosticsReportPanel.h
Editor/src/SagaEditor/Diagnostics/EditorDiagnosticsService.cpp
Editor/src/SagaEditor/Diagnostics/DiagnosticNavigation.cpp
```

Expected runtime/server files:

```txt
Engine/include/SagaEngine/Diagnostics/RuntimeDiagnostic.hpp
Engine/include/SagaEngine/Diagnostics/RuntimeDiagnosticSink.hpp
Engine/include/SagaEngine/Diagnostics/ServerDiagnostic.hpp
Engine/include/SagaEngine/Diagnostics/HealthSnapshot.hpp
Engine/src/SagaEngine/Diagnostics/RuntimeDiagnostic.cpp
Engine/src/SagaEngine/Diagnostics/ServerDiagnostic.cpp
```

Expected Forge/Prism files:

```txt
Tools/Forge/include/Forge/Diagnostics/ForgeDiagnostic.hpp
Tools/Forge/include/Forge/Diagnostics/DiagnosticCollector.hpp
Tools/Forge/include/Forge/Reports/BuildReport.hpp
Tools/Forge/include/Forge/Publish/PublishReport.hpp
Tools/Prism/include/Prism/Diagnostics/PrismDiagnostic.hpp
Tools/Prism/include/Prism/Reports/PrismReport.hpp
```

Expected reports:

```txt
<Project>/Build/Reports/build_report.json
<Project>/Build/Reports/diagnostics_report.json
<Project>/Build/Reports/publish_report.json
<Project>/Build/Reports/prism_stale_report.json
<Project>/Build/Reports/prism_boundary_report.json
<Project>/Build/Reports/runtime_diagnostics.json
<Project>/Build/Reports/server_diagnostics.json
```

---

## 34. Decision Summary

Preserve these decisions:

```txt
Diagnostics are structured evidence, not strings.
Diagnostic contracts may live in SagaShared.
Diagnostic emission belongs to each subsystem.
Diagnostic presentation belongs to product/editor/CLI consumers.
Authoring profiles change presentation depth, not diagnostic truth.
Publish-blocking diagnostics must never be hidden.
Diagnostics must support source/resource/artifact/package/build references.
SDE, Forge, Prism, AssetPipeline, Scripting, Collaboration, Runtime, and Server emit their own diagnostics.
Reports must be machine-readable and versioned.
Editor navigation requires source maps and resource refs.
Runtime/server diagnostics must support long-running reliability and startup package validation.
Reports must redact secrets.
```

The end goal is simple:

```txt
When something fails, Saga should say exactly what failed, where it failed, why it failed, what owns the fix, and what action is safe next.
```

Anything less is just making the user debug the architecture by smell.
