# SagaEngine Test Suites

> Last updated: 2026-06-02
> Status: focused gates with heavy opt-in policy

This document defines the local SagaEngine test suite names exposed through
Forge. These suite names are not the complete raw CTest label inventory;
labels such as `product`, `package`, `ui`, and `collaboration` may appear in
CTest registration outside the stable Forge suite table.

Quick entry: [README.md](README.md) summarizes the current gate policy.

Repository gate entry points:

```sh
scripts/build-default --dry-run
scripts/test-taxonomy --check
scripts/verify-local
```

These commands define the current structural build/test baseline. They do not
prove a full rebuild or raw full CTest pass. Use `scripts/verify-local
--with-build` and `scripts/verify-local --with-tests` only when intentionally
running the heavier local checks.

Use single-job execution first on this machine:

```sh
forge test --suite architecture --jobs 1
forge test --suite unit --jobs 1
forge test --suite runtime --jobs 1
forge test --suite server --jobs 1
forge test --suite asset --jobs 1
forge test --suite editor --jobs 1
forge test --suite tools --jobs 1
forge test --suite all-safe --jobs 1
```

Full test health is unverified unless every required suite is actually run and passes.
CTest registration and label presence alone are not pass evidence; focused
entries that are registered but absent from the current build tree must be built
and run before they support a suite claim.

Phase 9E defines the normal local gate as raw CTest with heavy labels excluded:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -LE "stress|slow|load|long-running|benchmark|perf" --output-on-failure --timeout 120 -j 1
```

This normal local gate passed 36/36 on 2026-05-26. It is not raw full CTest and
does not prove `ReplicationTests`, `StressTests`, stress, slow, load,
long-running, benchmark, or perf coverage.

## Block B SagaProjectKit Focused Entry

Phase 13-19 product/project truth evidence is outside raw CTest. It uses the
standalone C# SagaProjectKit tool and focused Python CLI tests:

```sh
dotnet build Tools/SagaProjectKit/SagaProjectKit.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py
```

`test_sagaprojectkit_cli.py` covers valid `.sagaproj` validation, missing
required fields, unsupported schema versions, project-relative path rules,
missing referenced profile paths, deterministic exit codes, read-only manifest
loading, stable resolve output, doctor checks, invalid profile JSON, and
`samples/MultiplayerSandbox` validate/resolve/doctor success.

This entry does not launch runtime or server processes, stage packages, open
the editor, run SagaLaunchLab, start Phase 20, or replace existing
`saga.project.json` behavior.

## Block C MultiplayerSandbox Preview Entry

Phase 20-25 server-only preview evidence uses one native test target and one
standalone C# launcher test:

```sh
cmake --build build/RelWithDebInfo-0.0.9 --target MultiplayerSandboxHeadless MultiplayerSandboxHeadlessTests -j 1
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'MultiplayerSandboxHeadlessTests' --output-on-failure --timeout 30 -j 1
dotnet build Tools/SagaLaunchLab/SagaLaunchLab.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaLaunchLab/tests/test_sagalaunchlab_cli.py
```

`MultiplayerSandboxHeadlessTests` covers the bounded server-side movement
proof and report creation. SagaLaunchLab tests cover server process launch,
invalid executable failure, timeout failure, and acceptance output with client
preview stages recorded as deferred.

## Block D SagaProbe Focused Entry

Phase 26-28 diagnostics visibility evidence is outside raw CTest. It uses the
standalone C# SagaProbe CLI and focused Python CLI tests:

```sh
dotnet build Tools/SagaProbe/SagaProbe.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaProbe/tests/test_sagaprobe_cli.py
```

`test_sagaprobe_cli.py` covers Engine operational summaries, reliability
summaries, MultiplayerSandboxHeadless summaries, SagaLaunchLab launch and
acceptance summaries, invalid/missing/unsupported input failures, deterministic
comparison output, changed metrics, added diagnostics, missing sections, latest
report selection, and stable summary output.

This entry does not add editor panels, dashboards, telemetry upload, runtime
launch behavior, package assembly, or release publication work.

## Block E SagaPackager Focused Entry

Phase 29-33 package proof evidence is outside raw CTest. It uses the standalone
C# SagaPackager CLI and focused Python CLI tests:

```sh
dotnet build Tools/SagaPackager/SagaPackager.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaPackager/tests/test_sagapack_cli.py
```

`test_sagapack_cli.py` covers package profile validation, invalid schema and
role diagnostics, missing profile and launch profile diagnostics, deterministic
staging, source project read-only behavior, output root safety, evidence-gate
pass and block behavior, packaged server/headless smoke, client packaged launch
deferral, and missing input handling.

This entry does not add setup bundle generation, updater behavior, certificate
seal flow, archive packing, marketplace upload, hosted deployment, ClientHost
migration, C# gameplay, node-authoring, or Phase 34 work.

## Block F SagaScript / SagaWeaver Focused Entry

Phase 34-43 C# scripting evidence is outside raw CTest for the CLI portion. It
uses the existing standalone C# SagaScript tool and focused Python CLI tests:

```sh
dotnet build Tools/SagaScript/SagaScript.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaScript/tests/test_sagascript_cli.py
python3 Tools/SagaScript/tests/test_sagascript_toolchain_check.py
```

The focused CLI tests cover legacy binding manifests, compile artifact
emission, two-axis `[SagaBehavior]` analysis, mixed level/domain diagnostics,
runtime binding metadata, source-map and projection output, opaque read-only
unsupported regions, no-edit source byte preservation, and preview-only string
literal patch output.

Optional native runtime scripting evidence remains focused:

```sh
cmake --build build/RelWithDebInfo-0.0.9 --target SagaScriptRuntimeBridge CSharpScriptHostTests CSharpGameplayProofTests ScriptBindingRuntimeTests -j 1
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'CSharpScriptHostTests|CSharpGameplayProofTests|ScriptBindingRuntimeTests' --output-on-failure -j 1
```

This entry does not add editor block UI, broad runtime/server gameplay changes,
general C# conversion, live code reload, debug tooling, or editor authoring
phase work.

## Block G Editor Authoring Spine Focused Entry

Phase 44-49 editor authoring evidence is native and focused:

```sh
cmake --build build/RelWithDebInfo-0.0.9 --target EditorAuthoringSpineTests SagaArchitectureTests -j 1
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'EditorAuthoringSpineTests|EditorQtPublicAbiBoundaryTests' --output-on-failure -j 1
```

`EditorAuthoringSpineTests` covers minimal `.sagaproj` display-subset loading,
subset parse diagnostics, read-only project/artifact status, deterministic
source-hash freshness states, read-only high/low projection views, patch
preview review with no apply action, and behavior inspector binding metadata.

This entry does not wire product-shell startup, add Qt panels, run SagaScript
from the editor, mutate C# files, generate script artifacts, edit scene/entity
data, or start collaboration work.

## Block H Collaboration MVP Focused Entry

Phase 50-56 collaboration evidence is native and focused:

```sh
cmake --build build/RelWithDebInfo-0.0.9 --target CollaborationModelTests EditorAuthoringSpineTests SagaArchitectureTests -j 1
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'CollaborationModelTests|EditorAuthoringSpineTests|EditorQtPublicAbiBoundaryTests' --output-on-failure -j 1
```

`CollaborationModelTests` covers deterministic local workspace identity,
privacy-limited identity metadata, local session ids, generated presence
reports that do not mutate project files, artifact metadata locks,
deterministic semantic transaction order, cursor/presence movement exclusion
from semantic transactions, metadata-only review operations, rejection of
unknown actor, locked artifact, stale source hash, missing target, unsupported
operation, out-of-order operation, deterministic conflict reports, and a
backend-neutral Team Room dashboard model.

This entry does not add editor UI, Qt public API, live shared editing, hosted
workspace services, login identity, security enforcement, source-file mutation,
graph mutation, runtime/server behavior, package/publish behavior, or Phase 57
work.

## Block I View Capability / Product Honesty Focused Entry

Phase 57-59 product honesty evidence uses two standalone C# CLIs and focused
Python tests:

```sh
dotnet build Tools/SagaViewKit/SagaViewKit.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaViewKit/tests/test_sagaviewkit_cli.py
dotnet build Tools/SagaDocGuard/SagaDocGuard.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaDocGuard/tests/test_sagadocguard_cli.py
```

`test_sagaviewkit_cli.py` covers built-in profile emission, valid/invalid view
capability manifests, Simple View rejection of unsupported edit/source rewrite
claims, Pro View source-link capability, deterministic reports, opaque
disclosure requirements, and hidden advanced-region detection.

`test_sagadocguard_cli.py` covers positive forbidden-claim rejection, required
non-claim detection, missing evidence failures, reviewed non-claim matches,
raw-full-CTest wording rejection without evidence, deterministic reports, and
read-only docs scanning.

This entry does not add editor UI, Qt public API, source rewriting, graph
editing, patch application, runtime graph interpretation, hosted collaboration,
release-readiness behavior, or Phase 60 work.

## Block J Technical Preview Candidate / Distribution Focused Entry

Phase 60-65 Technical Preview candidate evidence uses the standalone C#
SagaPreviewGate CLI plus existing focused tool and native regressions:

```sh
dotnet build Tools/SagaPreviewGate/SagaPreviewGate.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaPreviewGate/tests/test_sagapreviewgate_cli.py
```

`test_sagapreviewgate_cli.py` covers quickstart path checks, deterministic
MVP acceptance evidence ordering, missing evidence diagnostics, honest
unavailable build-matrix rows, Technical Preview package file checks, and
closure acceptance/blocking from local evidence.

The full Block J verification also keeps the focused regressions for
SagaProjectKit, SagaLaunchLab, SagaProbe, SagaPackager, SagaScript,
SagaViewKit, SagaDocGuard, EditorAuthoringSpineTests, CollaborationModelTests,
and EditorQtPublicAbiBoundaryTests.

This entry does not run raw full CTest, heavy stress, long soak, bot swarms,
real transport stress, editor UI, Qt public API expansion, source-changing
patch behavior, graph editing, hosted collaboration, commercial distribution,
or Target 2 / Small-Team Alpha / Phase 66 work.

## Target 2 / Small-Team Alpha Block A Alpha Opening Focused Entry

Phase 66-68 Small-Team Alpha opening evidence uses the standalone C#
SagaAlphaGate CLI plus existing Technical Preview honesty regressions:

```sh
dotnet build Tools/SagaAlphaGate/SagaAlphaGate.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaAlphaGate/tests/test_sagaalphagate_cli.py
```

`test_sagaalphagate_cli.py` covers Technical Preview closure evidence
requirements, deterministic allowed/blocked alpha claim reports, the Small-Team
Alpha scenario matrix, manual/later-phase workflow classification, v0 budget
reports, missing measurements, and deterministic budget violations.

The focused regression set also keeps SagaPreviewGate, SagaDocGuard,
SagaProjectKit, SagaScript, SagaViewKit, EditorAuthoringSpineTests,
CollaborationModelTests, and EditorQtPublicAbiBoundaryTests.

This entry does not implement editor UI, block editing, source-changing patch
behavior, collaboration persistence, package profile expansion, launch profile
matrix work, performance benchmarking, product beta status, release candidate
status, enterprise readiness, or Target 3 / Enterprise-Evolvable Foundation / Phase 96 work.

## Target 2 / Small-Team Alpha Block B Policy Mini-Block Focused Entry

Before Phase 69 starts, the policy mini-block uses SagaDocGuard evidence only:

```sh
dotnet build Tools/SagaDocGuard/SagaDocGuard.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaDocGuard/tests/test_sagadocguard_cli.py
Tools/SagaDocGuard/sagadocguard scan --docs docs --evidence-root . --out Build/SmallTeamAlpha/docguard_report.json
```

`test_sagadocguard_cli.py` now requires the six Phase 69-75 policy documents:
gameplay node library v1, source patch application, editor UI strategy, client
preview and runtime UI strategy, generated files policy, and artifact contracts
v1.

This entry does not add native targets, node extraction, gameplay node APIs,
source-changing patch behavior, editor UI, Qt public ABI, runtime/server/client
behavior, `ClientHost` migration, generated code rewrites, or Target 3 / Enterprise-Evolvable Foundation work.

## Target 2 / Small-Team Alpha Block B Phase 69-71 Focused Entry

Phase 69-71 C# node library and projection evidence uses SagaScript and
SagaViewKit focused CLI tests:

```sh
dotnet build Tools/SagaScript/SagaScript.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaScript/tests/test_sagascript_cli.py
dotnet build Tools/SagaViewKit/SagaViewKit.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaViewKit/tests/test_sagaviewkit_cli.py
```

`test_sagascript_cli.py` covers `[SagaLibrary]` extraction, Gameplay.High
`[SagaNode]` extraction as `ProjectionOnly`, Gameplay.Low extraction as
`Deferred`, duplicate and missing node ids, mixed domain/level diagnostics,
deterministic `node_library_report.json`, source preservation, projection
capability fields, and deferred read-only disclosure.

`test_sagaviewkit_cli.py` covers Simple View rejection of editable deferred
nodes and hidden deferred metadata.

This entry does not implement source-changing patch behavior, rollback,
undo/redo, C# compatibility profile v2, runtime/server/client behavior,
package/publish integration, editor UI, Qt public ABI, Target 2 / Small-Team Alpha Block C, or
Target 3 / Enterprise-Evolvable Foundation work.

## Target 2 / Small-Team Alpha Block B Phase 72 Focused Entry

Phase 72 `ReplaceStringLiteral` apply evidence uses SagaScript focused CLI
tests:

```sh
nix-shell --run "dotnet build Tools/SagaScript/SagaScript.csproj --configuration Release --nologo --verbosity minimal -m:1"
nix-shell --run "python3 Tools/SagaScript/tests/test_sagascript_cli.py"
```

`test_sagascript_cli.py` covers successful exact-span source mutation,
outside-span byte preservation, stale hash rejection, deterministic missing
source failure, `expectedOldText` mismatch rejection, read-only/opaque/deferred
and non-string target rejection, backup creation, rollback on failed post-check,
comment/whitespace/using-order preservation, stale artifact reporting without
regeneration, and the existing non-mutating `patch-preview` behavior.

This entry does not add patch operations beyond `ReplaceStringLiteral`,
multi-file patching, editor-side C# writes, undo/redo, review UI, C# profile v2,
runtime/server/client behavior, package/publish integration, Target 2 / Small-Team Alpha Block C,
or Target 3 / Enterprise-Evolvable Foundation work.

## Target 2 / Small-Team Alpha Block B Phase 73 Focused Entry

Phase 73 diff, review, and rollback evidence uses SagaScript focused CLI tests:

```sh
nix-shell --run "dotnet build Tools/SagaScript/SagaScript.csproj --configuration Release --nologo --verbosity minimal -m:1"
nix-shell --run "python3 Tools/SagaScript/tests/test_sagascript_cli.py"
```

`test_sagascript_cli.py` covers non-mutating `patch-diff`, diff stale-hash and
target-policy rejection, report-only `patch-review` approval/rejection,
malformed or failed diff review rejection, strict `patch-rollback` exact-byte
restore, rollback source-hash and backup-hash rejection, missing or failed
apply report rejection, rollback post-check recovery to current bytes, stale
artifact reporting without regeneration, and Phase 72 regression behavior.

This entry does not add patch operations beyond `ReplaceStringLiteral`, a broad
undo/redo stack, redo mutation, editor-side C# writes, editor UI, Qt UI, graph
editing, C# profile v2, runtime/server/client behavior, package/publish
integration, Target 2 / Small-Team Alpha Block C, or Target 3 / Enterprise-Evolvable Foundation work.

## Suite Names

| Suite | CTest behavior | Notes |
|---|---|---|
| `architecture` | label regex `architecture` | Boundary and architecture checks. |
| `unit` | label regex `unit` | Unit tests. Some coverage is still coarse because `UnitTests` is a broad executable. |
| `runtime` | label regex `runtime` | Runtime and runtime-adjacent tests. |
| `server` | label regex `server` | Server-adjacent tests. Coarse until server tests are split further. |
| `networking` | label regex `networking` | Networking tests and integration coverage. |
| `replication` | label regex `replication` | Replication tests. Heavy replication entries may also be `slow` or `long-running`. |
| `asset` | label regex `asset` | Asset and package/manifest-adjacent tests. |
| `editor` | label regex `editor` | Editor tests and editor tool coverage. |
| `tools` | label regex `tools` | Tooling tests. |
| `integration` | label regex `integration` | Integration tests. Timing-sensitive entries are excluded from `all-safe`. |
| `stress` | label regex `stress` | Opt-in only. |
| `slow` | label regex `slow` | Opt-in only. |
| `all-safe` | label exclude `stress|slow|load|timing-sensitive|long-running` | Forge local safety profile. Phase 9E normal CTest excludes `stress|slow|load|long-running|benchmark|perf`. |

## Existing Label Behavior

Existing Forge label filtering is still supported:

```sh
forge test --label runtime --jobs 1
```

Do not combine `--suite` and `--label`; a suite is the SagaEngine-specific stable profile, while `--label` is the raw CTest label regex escape hatch.

## Focused Architecture Entries

`EditorQtPublicAbiBoundaryTests` is a focused CTest entry for Phase 6B:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1
```

It runs `SagaArchitectureTests --gtest_filter=EditorQtPublicAbiBoundaryTests.*`
and is labeled `unit;architecture;editor`. The guard scans public Editor
headers and `Apps/Saga/SagaEditorModule.h` for new Qt ABI exposure outside the
current explicit allowlist. It does not link Qt and does not prove Editor public
headers are Qt-free.

## Focused Diagnostics Entries

Phase 2 adds focused diagnostics report and server observability entries,
Phase 3 adds focused crash-context, reliability, HealthRule, HealthSeverity,
and lifetime leak diagnostic coverage, and Phase 5 adds explicit memory/resource
snapshot coverage:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R "DiagnosticReportTests|DiagnosticReliabilityTests|DiagnosticMemoryResourceTests|ZoneServerDiagnosticsTests" --output-on-failure -j 1
```

`DiagnosticReportTests` covers `DiagnosticReport`,
`DiagnosticReportWriter`, `health_snapshot`, `lifetime_leaks`,
`operational_snapshot`, deterministic JSON order, deterministic write failures,
and `WriteOperationalReport(path)`.

`DiagnosticReliabilityTests` covers `CrashReport`, `CrashContext`,
`manual_crash_report`, `reliability_failure_report`, deterministic crash report
JSON, deterministic crash report write failures, health rule pass/fail and
missing metric behavior, `HealthSeverity`, recent log capture, lifetime leak
candidates, and lifetime leak summaries by type and owner system.

`DiagnosticMemoryResourceTests` covers `MemoryTracker`, `MemorySnapshot`,
`MemoryScope`, `ResourceTracker`, `ResourceSnapshot`, deterministic
over-removal and overflow behavior, invalid and double resource release,
active resource summaries by type and owner system, and memory/resource report
sections in operational and reliability reports.

`ZoneServerDiagnosticsTests` covers optional `SagaDiagnostics` injection and
local metrics for `server.tick.count`, `world.entities.active`,
`server.movement.accepted_inputs`, `server.movement.rejected_inputs`, and
`server.packets.rejected`. Phase 3 extends this entry with `server.tick.ms` and
`server.tick.budget_overruns` reliability observability. The entry also
verifies movement input still queues before tick and mutates only during tick.

Phase 5 non-claims: these tests do not prove production crash safety, unsafe OS
signal/SEH crash handlers, stack traces, a custom allocator, allocation hooks,
OS memory profiling, OS handle enumeration, full leak detection, NetworkChaos,
StateValidation, FaultBoundary, production readiness, or complete raw CTest health.

Phase 6 adds focused NetworkChaos policy coverage:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R NetworkChaosLayerTests --output-on-failure -j 1
```

`NetworkChaosLayerTests` covers disabled pass-through behavior, deterministic
drop decisions for a fixed seed, duplicate-once output, explicit tick-driven
defer release, deferred queue cap behavior, optional `net.chaos.*` diagnostics
metrics, and missing-diagnostics determinism.

Phase 6 non-claims: this is not real socket-level packet manipulation, OS
network shaping, a real network BotClient, SagaChaosLab, MMO-scale network
stress, production network resilience, unbounded queues, sleep-based latency
testing, heavy stress, or complete raw CTest health.

Phase 7 adds focused SagaChaosLab profile-runner coverage:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R SagaChaosLabTests --output-on-failure -j 1
```

`SagaChaosLabTests` covers the built-in bounded smoke profile, deterministic
profile loading, exact `NetworkChaosConfig` field mapping, schema-version
failure, missing required fields, invalid probability fields and probability
sum, unsafe actor/tick/duration/defer queue bounds, unsupported profile fields,
manual/heavy execution blocking without opt-in, and `chaos_report.json`
creation with deterministic `net.chaos.*` summary metrics.

Safe Phase 7 regression slice:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'SagaChaosLabTests|NetworkChaosLayerTests|SagaStressArenaTests' --output-on-failure -j 1
```

Phase 7 non-claims: this is still direct/local and single-process. It does not
prove production network chaos, a real internet condition emulator, OS-level
network shaping, a distributed chaos cloud, a bot swarm, MMO-scale stress, full
network resilience, release readiness, beta product readiness, socket transport
chaos, reconnect storms, external services, or complete raw CTest health.

## Focused Server Lifecycle Diagnostics

Phase 8 adds a focused direct/local lifecycle diagnostics target:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'ServerLifecycleDiagnosticsTests|ZoneServerDiagnosticsTests|DiagnosticReportTests|DiagnosticReliabilityTests' --output-on-failure -j 1
```

`ServerLifecycleDiagnosticsTests` covers bounded tracker defaults, deterministic
event payload ordering, event/record caps and dropped counts, direct/local
server start and shutdown lifecycle events, session connect/disconnect records,
entity create/destroy records, explicit-duration `TickSlow`, explicit-only
`LifetimeLeakDetected`, empty report serialization, diagnostics-absent entity
behavior, and idempotent snapshot/report construction.

Phase 8 non-claims: no production account/session service, production-ready MMO
server, full replication lifecycle correctness, full replication observer
lifecycle, real transport stress, bot swarm, MMO-scale stress, release
readiness, beta product status, or raw full CTest pass claim.

## Phase 9 Real Transport Smoke Deferral

The product-roadmap Phase 9 real transport smoke is deferred with evidence. The
repository has a real `ZoneServer` TCP accept seam, but it does not yet expose a
stable client/test transport helper, handshake response writer, or server-frame
builder for a safe localhost smoke.

Safe Phase 9 regression evidence remains the existing direct/local diagnostics
and chaos slice:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'NetworkChaosLayerTests|SagaChaosLabTests|SagaStressArenaTests|ZoneServerDiagnosticsTests|ServerLifecycleDiagnosticsTests' --output-on-failure --timeout 30 -j 1
```

This regression slice does not prove real transport stress, production network
readiness, replication correctness, reconnect behavior, bot swarm behavior,
MMO-scale stress, release readiness, beta product readiness, or raw full CTest
health.

## Phase 10 SagaStateCheck Foundation

Phase 10 adds focused deterministic state validation contract coverage:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R SagaStateCheckTests --output-on-failure --timeout 30 -j 1
```

`SagaStateCheckTests` covers matching snapshots, position mismatch, missing
entity, extra entity, ownership mismatch, checksum/report determinism,
explicit empty snapshot handling, and deterministic `state_check_report` JSON
shape.

This does not prove full replication correctness, live server-wide
authoritative movement snapshot coverage, anti-cheat, production desync
recovery, large-world validation, or real transport validation.

## Phase 11 FaultBoundary Contract

Phase 11 adds focused minimal fault classification and diagnostics report
coverage:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'FaultBoundaryTests|DiagnosticReportTests' --output-on-failure --timeout 30 -j 1
```

`FaultBoundaryTests` covers policy classification, sorted metadata, diagnostics
storage, additive operational report serialization, empty fault report shape,
bounded retention, and reported-only recovery actions.

This does not prove production fault tolerance, automatic crash recovery, full
fault isolation, subsystem fake recovery, or broad Runtime/Server recovery.

## Phase 12 Diagnostics Closure Checkpoint

Phase 12 closure evidence should use the focused diagnostics regex:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'SagaStateCheckTests|FaultBoundaryTests|NetworkChaosLayerTests|SagaChaosLabTests|SagaStressArenaTests|ServerLifecycleDiagnosticsTests|DiagnosticReportTests' --output-on-failure --timeout 60 -j 1
```

This closes Block A only as a diagnostics foundation for Technical Preview
product work. It does not prove production readiness, production networking,
production MMO server behavior, real transport stress, full replication
correctness, full fault tolerance, release readiness, beta product readiness,
enterprise readiness, or raw full CTest health.

## Focused SagaStressArena Entry

Phase 4 adds a safe focused tool test for the first bounded stress/soak
diagnostics artifact proof:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R SagaStressArenaTests --output-on-failure -j 1
```

`SagaStressArenaTests` covers the `SagaStressArena` direct local ZoneServer
harness, smoke tier defaults, `mini_soak` bounds, manual-only `heavy` tier
behavior, deterministic invalid config failures, deterministic invalid report
directory failure, and creation of:

```txt
direct_zone_stress_operational_snapshot.json
direct_zone_stress_reliability_report.json
direct_zone_stress_lifetime_leaks.json
```

The test verifies stable report fields such as `server.tick.count`,
`server.tick.ms`, `world.entities.active`, movement accepted/rejected metrics,
packet rejection metrics, health rule names, scenario/tier metadata, and
lifetime leak summary content. Phase 5 extends this proof with deterministic
memory/resource snapshot fields in the same report artifacts. It does not
compare full raw JSON bytes because generation sequence, thread id, or
environment-specific fields may vary.

Phase 4 non-claims: this is a local ZoneServer harness, not full network
stress. It does not prove real bot swarm behavior, MMO-scale load,
load/performance readiness, production readiness, MemoryTracker/ResourceTracker,
NetworkChaos, StateValidation, FaultBoundary, default heavy stress, or full raw
CTest health.

Phase 5 updates that non-claim: `SagaStressArena` now proves explicit
memory/resource snapshot report integration, but it still does not prove real
OS memory profiling, OS handle enumeration, full memory leak detection,
load/performance readiness, production readiness, NetworkChaos,
StateValidation, FaultBoundary, or complete raw CTest health.

Phase 6 adds `direct_zone_packet_chaos_smoke` to the same safe direct harness.
`SagaStressArenaTests` verifies that the chaos smoke operational report contains
deterministic `net.chaos.*` metrics. This remains direct/local and does not
prove real transport chaos or heavy network stress.

## Focused Publish And Package Entries

Phase 10 uses focused package/publish/runtime tests as product packaging reality
evidence:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R "PackageManifestWriterTests|AssetManifestWriterTests|AssetIdentityManifestWriterTests|AssetIdentityRuntimeContractTests|GeneratedRuntimeSmokeManifestTests|GeneratedRuntimeSmokePackageTests|RuntimePackageSmokeTests|RuntimeAssetBootstrapTests|RuntimeAssetCatalogTests|RuntimeAssetStartupBootstrapTests|SagaPackageStagingTests|SagaPublishReadinessTests" --output-on-failure -j 1
```

This focused gate proves current report-first publish readiness, package
staging, generated manifest, RuntimeSmoke package, and runtime asset bootstrap
compatibility. It does not prove release readiness, full AssetPipeline
cook/import, ClientHost package consumption, RuntimeServiceRegistry asset
service, raw full CTest health, stress/load readiness, or heavy replication
readiness.

## Phase 11 Recovery Classification

Phase 11 closes the recovery roadmap as Foundation Established. This
classification accepts the Phase 9 normal local gate and Phase 10 focused
package/publish/runtime compatibility proof as foundation evidence. It does not
upgrade raw full CTest, `StressTests`, or heavy `ReplicationTests` into passing
evidence.

## SagaScript Phase 74-75 Evidence Tests

Phase 74-75 adds focused CLI coverage in:

```sh
python3 Tools/SagaScript/tests/test_sagascript_cli.py
python3 Tools/SagaPackager/tests/test_sagapack_cli.py
python3 Tools/SagaAlphaGate/tests/test_sagaalphagate_cli.py
```

The SagaScript suite covers compatibility profile classification,
non-mutating profile generation, runtime binding node metadata, ProjectionOnly
and Deferred runtime-proof summaries, validation failure for RuntimeBacked nodes
without explicit evidence, and patch report mutation flag validation.

The SagaPackager suite covers unchanged publish-check behavior when script
validation is absent, accepted optional script validation evidence, and blocked
publish-check output for failed script validation.

The SagaAlphaGate suite covers report-only script evidence recording and blocked
script evidence when SagaScript validation is failed or runtime proof is
missing. These tests do not prove runtime node execution, runtime gameplay,
server gameplay, Editor UI, Qt UI, graph editing, full visual scripting,
arbitrary C# to blocks, Phase 76+, or Target 3 / Enterprise-Evolvable Foundation work.

## Editor Workflow Usability Phase 76-81 Tests

Phase 76-81 extends `EditorAuthoringSpineTests` with focused backend-neutral
workflow coverage:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'EditorAuthoringSpineTests|EditorQtPublicAbiBoundaryTests' --output-on-failure -j 1
```

The suite covers missing and invalid `.sagaproj` diagnostics, project browser
section links, script artifact evidence in the behavior inspector,
`ProjectionOnly` and `Deferred` runtime-proof disclosure, missing runtime
evidence, diagnostics panel severity grouping and missing summary handling,
report-backed patch review state, disabled editor-owned apply/rollback actions,
scene/asset missing source truth, Simple/Pro/C# navigation state preservation,
read-only C# source visibility, and the public Qt ABI guard.

C# source remains canonical. SagaScript owns source mutation. The Editor does
not write C# directly and does not apply or rollback patches directly. This
coverage does not prove Editor UI, Qt panel implementation, full editor MVP,
full visual scripting, arbitrary C# to blocks, graph editing, drag/drop node
editing, runtime gameplay, server gameplay, ClientHost work, asset import,
scene editing, prefab editing, entity placement, or Target 3 / Enterprise-Evolvable Foundation work.

## Block D Phase 82-86 Evidence Tests

Phase 82-86 extends focused CLI coverage in:

```sh
python3 Tools/SagaPackager/tests/test_sagapack_cli.py
python3 Tools/SagaLaunchLab/tests/test_sagalaunchlab_cli.py
python3 Tools/SagaAlphaGate/tests/test_sagaalphagate_cli.py
```

The SagaPackager suite covers read-only asset workflow validation, duplicate
asset ids, missing asset roots, placeholder-only MultiplayerSandbox assets,
missing accepted asset source truth as `MissingSourceOfTruth`, generated
package asset manifest evidence, and the package profile matrix for the
existing `technical-preview-server-headless` profile.

The SagaLaunchLab suite covers the launch profile matrix, the bounded
server/headless runnable row, missing profile diagnostics, invalid executable
diagnostics, and deferred one-client/two-client expectations because no bounded
ClientHost/runtime preview report seam exists.

Target 4 / Source Truth Foundation Phase 141-148 extends focused CLI coverage in:

```sh
python3 Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py
python3 Tools/SagaPackager/tests/test_sagapack_cli.py
python3 Tools/SagaLaunchLab/tests/test_sagalaunchlab_cli.py
```

The added coverage proves report-only component metadata ownership,
backend-neutral editor inspection/read evidence, future-readable Runtime
read-model audits, planning-only Runtime readiness, package profile
source-truth alignment without staging, launch profile source-truth alignment
without launching, and deterministic source-truth scenario aggregation. It does
not prove Runtime gameplay, Server gameplay, Client Preview, ClientHost, Editor
UI, Qt UI, asset import, asset cook, C# mutation, raw full CTest, heavy stress,
soak, bot swarm, or real transport stress.

Target 4 / Source Truth Foundation Phase 149-155 extends focused SagaProjectKit CLI coverage in:

```sh
python3 Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py
```

The added coverage proves report-only Client Preview prerequisite auditing,
ClientHost boundary planning, asset import/cook deferral, broad test health
preflight classification, deterministic Target 4 / Source Truth Foundation evidence matrix output, and
closure blocking when required source-truth evidence is missing. It keeps
Client Preview, ClientHost, asset import, asset cook, raw full CTest, heavy
stress, soak, bot swarm, and real transport proof as deferred or unclaimed
evidence only.

Target 5 / Runtime Read Seam / Client Preview Planning Phase 156-160 extends the same focused SagaProjectKit CLI suite with
`runtime-readiness-v2` coverage:

```sh
python3 Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py
```

The added coverage proves deterministic report-only Runtime read seam readiness
classification, `MissingEvidence` for absent or incomplete `Build/SourceTruth`
inputs, `Blocked` for failed required evidence, `PartiallyProven` for current
partial Target 4 / Source Truth Foundation-style evidence, and `ReadyForImplementationPlanning` without
claiming Runtime, ClientHost, or Client Preview implementation. The command
preserves `localOnly=true`, `networkExposure=None`, `mutatesSource=false`, and
`enforcement=ReportOnly`.

Phase 161-165 Client Preview / Runtime read seam planning coverage is in the
same focused SagaProjectKit suite:

```sh
python3 Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py
```

The added coverage proves deterministic ClientHost Preview ownership boundary
reports, `MissingEvidence` for missing Runtime readiness v2 input,
server-only classification for `local-server-headless`, deferred
`client-preview-local-no-network` planning, no-process-launch evidence,
no-network local mode non-claims, diagnostics shell aggregation, missing input
blocking, and blocker matrix behavior for current partial evidence and
synthetic ready planning evidence. The tests do not prove ClientHost
implementation, Client Preview implementation, Runtime gameplay, Server
gameplay, Runtime adapters, LaunchLab execution, sockets, transport sessions,
asset import, asset cook, multiplayer proof, or network proof.

Phase 166-170 extends the same suite with report-only planning coverage for
the first possible Client Preview implementation path:

```sh
python3 Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py
```

The added coverage proves deterministic minimal Runtime read seam planning,
missing required evidence blocking, ClientHost preview shell planning without
ClientHost implementation, package/launch preview alignment without staging or
launch execution, editor-less workflow planning without Playable Editor claims,
and preview evidence gate handling for current partial, missing, blocked, and
synthetic ready planning evidence. The tests do not prove Runtime read seam
implementation, ClientHost implementation, Client Preview implementation,
Runtime gameplay, Server gameplay, LaunchLab execution, sockets, networking,
asset import, asset cook, multiplayer proof, or network proof.

Phase 171-175 extends the same suite with the Client Preview prerequisite
layer:

```sh
python3 Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py
```

The added coverage proves deterministic asset import/cook prerequisite
auditing, generated/package artifact evidence-only classification, accepted
asset reference propagation, missing Runtime asset consumption seam reporting,
Runtime projection freshness gating with generated projections non-canonical,
six plan-only Client Preview regression fixtures, focused SagaProjectKit CLI
coverage rows, and unclaimed raw full CTest, heavy stress, and real transport
proof rows. It does not prove Runtime asset consumption, Runtime gameplay,
ClientHost implementation, Client Preview implementation, asset import, asset
cook, process launch, sockets, networking, multiplayer proof, or network proof.

The SagaAlphaGate suite covers performance budget missing measurements,
deterministic budget violations, and the Phase 86 gameplay expansion blocker
report. These tests do not prove asset import, asset cook, scene/entity source
truth, ClientHost preview, runtime gameplay, server gameplay, multi-client
stress, production performance readiness, raw full CTest, or Phase 87+ work.

## Block E Phase 87-92 Collaboration Tests

Phase 87-92 extends focused native coverage in:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'CollaborationModelTests|EditorQtPublicAbiBoundaryTests' --output-on-failure -j 1
```

`CollaborationModelTests` covers durable workspace state serialization and
parsing, deterministic invalid-state diagnostics, private identity omission,
persistent lock target scopes, stale lock classification, artifact comments on
artifact/behavior/node/source-span/patch/diagnostic targets, comment
resolve/reopen transitions, review request state transitions,
`ApprovedMetadataOnly` review approval, stale review detection, local role
checks for dangerous operations, and Team Room summaries for local metadata.

This coverage does not prove live shared editing, hosted sync, account login,
security enforcement, source merge, CRDT, operational transform, editor UI, Qt
UI, public Qt ABI expansion, collaboration-driven C# source mutation,
SagaScript patch behavior changes, Runtime gameplay, Server gameplay,
ClientHost work, Phase 93+, or Target 3 / Enterprise-Evolvable Foundation work.

## Block F Phase 93-95 Alpha Closure Tests

Phase 93-95 extends focused CLI coverage in:

```sh
python3 Tools/SagaAlphaGate/tests/test_sagaalphagate_cli.py
python3 Tools/SagaDocGuard/tests/test_sagadocguard_cli.py
```

The SagaAlphaGate suite covers `accept-alpha`, `evidence-matrix`, and
`close-alpha` reports, complete focused evidence fixtures, deterministic
missing-evidence blockers, deterministic evidence matrix output, closure
dependency on acceptance and matrix reports, and deferred gameplay/client/editor
rows remaining non-passing.

The SagaDocGuard suite covers Phase 94/95 required evidence docs, positive
forbidden-claim rejection, non-claim review context, and Target 3 / Enterprise-Evolvable Foundation boundary
wording. This coverage does not prove product beta, release candidate,
production readiness, raw full CTest, heavy stress, Runtime gameplay, Server
gameplay, ClientHost work, editor UI, Qt UI, or Phase 96 work.

## Known Limitations

- Phase 1 adds suite visibility; it does not prove all suites pass.
- Phase 6B adds an Editor public Qt exposure guard; it does not remove current
  allowed Qt leaks.
- Phase 1 does not refactor CMake target dependencies.
- The current `UnitTests` executable still groups many subsystems together, so several suite labels intentionally select coarse coverage.
- Some focused CTest entries can be registered before their executable has been
  built on demand in the local build tree.
- Under Forge `all-safe`, `stress`, `slow`, load, timing-sensitive, and
  long-running tests remain opt-in. The Phase 9E normal CTest gate uses the
  separate exclusion policy documented above.
- Raw full CTest remains unresolved after Phase 9D terminal/session instability
  and must not be reported as passing without a complete passing run.
- `ReplicationTests` is opt-in heavy because it is labeled
  `slow;long-running`; `StressTests` is opt-in heavy because it is labeled
  `stress;load;slow`.
