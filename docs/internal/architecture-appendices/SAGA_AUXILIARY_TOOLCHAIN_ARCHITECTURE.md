# SagaEngine Auxiliary Toolchain Architecture

## Document Status

**Status:** Proposed appendix / auxiliary toolchain idea bank
**Scope:** SagaEngine auxiliary tools required for productization, diagnostics visibility, project validation, launch orchestration, packaging, collaboration, view capability management, policy enforcement, stress testing, state validation, and documentation claim/evidence governance.
**Explicit exclusion:** SagaWeaver is intentionally excluded from this document because it has its own dedicated architecture document.
**Current appendix location:** `docs/internal/architecture-appendices/SAGA_AUXILIARY_TOOLCHAIN_ARCHITECTURE.md`
**Primary goal:** Preserve toolchain direction ideas: which non-SagaWeaver tools SagaEngine may need, why each tool exists, what risk it reduces, which language it might use, which external libraries may be appropriate, whether it should live inside the Saga monorepo or later become a separate repository, and how each tool should integrate with the engine.

This document is not current implementation truth, not a productization
checklist, and not evidence that the listed tools exist or are required for
current onboarding. Current tool evidence must come from focused tool contracts,
local reports, and testing evidence docs.

---

# 1. Executive Summary

SagaEngine is no longer just a C++ runtime. The intended product direction is:

```text
multiplayer-first engine
+ customizable editor
+ C# gameplay authoring
+ visual block projection
+ runtime diagnostics
+ packaging/publish proof
+ collaboration
+ project model
+ product-grade evidence
```

A single engine executable should not own all of this.

Some responsibilities should be implemented as tools because they are:

```text
build-time operations
validation operations
report readers
project/package validators
launch orchestrators
diagnostic analyzers
collaboration/session services
policy/evidence gates
```

This document defines the auxiliary tools needed around SagaEngine, excluding SagaWeaver.

The recommended toolchain is:

```text
Product/Core Proof Tools:
    SagaProjectKit
    SagaLaunchLab
    SagaPackager / SagaPublishGate
    SagaProbe

Reliability / Runtime Test Tools:
    SagaStressArena
    SagaChaosLab
    SagaStateCheck

Editor / Collaboration / Governance Tools:
    SagaViewKit
    SagaWorkspaceHub
    SagaPolicyKit
    SagaDocGuard
```

Do not implement all of these at once.

The first practical toolchain should be:

```text
SagaProjectKit
    ↓
SagaLaunchLab
    ↓
SagaProbe
    ↓
SagaPackager / SagaPublishGate
```

SagaWeaver should run in parallel as the C#/visual-authoring toolchain, but it is documented separately.

---

# 2. Core Toolchain Philosophy

## 2.1 Tools Exist To Reduce Product Risk

A tool should only exist if it reduces a real risk.

Bad reason:

```text
This sounds cool.
This could become a product one day.
This makes the repo look serious.
```

Good reason:

```text
Project validation is inconsistent.
Package proof is manual.
Diagnostics reports are unreadable.
Launch profiles are not reproducible.
Collaboration state cannot be audited.
Docs overclaim implementation status.
```

Every tool in this document must answer:

```text
What risk does this reduce?
What artifact does it produce?
What system consumes that artifact?
What is the smallest proof?
```

## 2.2 Tools Should Communicate Through Artifacts

Recommended integration pattern:

```text
Tool
    ↓ produces
JSON / manifest / report / diagnostic file
    ↓ consumed by
Editor / Runtime / Publish Gate / Collaboration / CI / Forge
```

Avoid:

```text
Tool directly includes private engine headers.
Runtime links editor-only tooling.
Editor panels implement their own validators.
Each tool invents its own schema style.
```

## 2.3 Monorepo First, Separate Repo Later

Default rule:

```text
Every Saga tool starts inside the SagaEngine monorepo.
```

A tool may become a separate repository only if:

```text
1. Its core is useful without Saga-specific engine internals.
2. Its CLI is stable.
3. Its JSON schema is versioned.
4. Its tests run independently.
5. Saga integration is an adapter, not a private-header dependency.
6. The tool has real users or reuse value outside Saga.
```

Do not create separate repositories prematurely. That creates maintenance overhead before the tool has proven value.

## 2.4 Language Selection Principles

Language choice should be boring and practical.

Use **C++** when:

```text
tool must use Saga C++ code directly
tool must integrate with SagaServer/SagaRuntime internals
tool is a runtime stress harness
tool needs existing CMake/CTest/GoogleTest integration
```

Use **C#** when:

```text
tool is a cross-platform CLI
tool manipulates JSON/YAML/manifests
tool orchestrates processes
tool integrates with scripting or editor workflows
tool benefits from strong typing and .NET ecosystem
```

Use **Rust** when:

```text
tool is a long-running service
security/concurrency matters
protocol boundaries matter
memory safety matters
the tool may eventually become an independent service
```

Use **Python** only when:

```text
tool is temporary glue
one-off migration script
developer-local report summarizer
prototype only
```

Avoid **Java** unless there is a specific JVM ecosystem reason. Saga has no current reason to introduce Java for these tools.

Avoid **C** for these tools. C is appropriate for low-level runtime/system code, not product tooling.

---

# 3. Tool Overview Table

| Tool | Primary Role | Recommended Language | Separate Repo Later? | First Priority |
|---|---|---:|---:|---:|
| SagaProjectKit | Project creation, validation, path resolution, project doctor | C# | Maybe | High |
| SagaLaunchLab | Local server/client preview orchestration | C# | No | High |
| SagaPackager / SagaPublishGate | Package creation, package smoke, publish readiness gate | C# + small C++ consumers | Maybe | High |
| SagaProbe | Diagnostics/crash/stress report reader and summarizer | C# first, optional C++/Qt UI later | Maybe | High |
| SagaStressArena | Server stress/soak harness, bots, lifecycle testing | C++ | No | Medium/High |
| SagaChaosLab | Network chaos scenarios and deterministic profiles | C++ core + C# CLI wrapper optional | No | Medium |
| SagaStateCheck | Server state validation, desync/checksum report analysis | C++ | No | Medium/Late |
| SagaWorkspaceHub | Collaboration session, presence, locks, transactions | Rust or C# | Maybe | Medium/Late |
| SagaPolicyKit | Permissions, project slices, source visibility, redaction | Rust or C# | Maybe | Late |
| SagaViewKit | Editor view capability manifests, abstraction ladder validation | C# | Maybe | Medium |
| SagaDocGuard | Documentation claim/evidence gate | C# | Yes, eventually | Medium |

---

# 4. Shared Artifact Conventions

All tools should follow common artifact conventions.

## 4.1 JSON Schema Versioning

Every JSON output should include:

```json
{
  "schemaVersion": 1
}
```

Recommended top-level fields:

```json
{
  "schemaVersion": 1,
  "tool": "SagaProjectKit",
  "toolVersion": "0.1.0",
  "generatedAtUtc": "2026-05-27T00:00:00Z",
  "status": "Passed",
  "diagnostics": []
}
```

## 4.2 Diagnostic Format

Common diagnostic shape:

```json
{
  "code": "SAGA_PROJECT_0001",
  "severity": "Error",
  "category": "ManifestValidation",
  "message": "Project manifest is missing required field 'projectId'.",
  "path": "Anhos.sagaproj",
  "line": 12,
  "column": 5
}
```

Severity levels:

```text
Trace
Info
Warning
Error
Fatal
```

## 4.3 Report Locations

Recommended output conventions:

```text
Build/
  Reports/
    project_validation_report.json
    launch_preview_report.json
    package_report.json
    publish_report.json
    diagnostics_summary.json
    docguard_report.json

  SagaScript/
    ... SagaWeaver artifacts, documented separately ...

diagnostics/
  reports/
    crash_*.json
    health_snapshot_*.json
    lifetime_leaks_*.json
    stress_*.json
```

## 4.4 Exit Codes

All tools should use consistent exit codes:

```text
0 = success
1 = validation failed
2 = invalid CLI usage
3 = missing input
4 = internal tool error
5 = unsupported schema/version
6 = external process failed
```

---

# 5. SagaProjectKit

## 5.1 Purpose

SagaProjectKit owns project-level creation, validation, path resolution, and project health checks.

It exists because Saga needs a canonical project object before editor, runtime, packaging, scripting, and collaboration can agree on the same truth.

## 5.2 What It Prevents

SagaProjectKit prevents:

```text
Editor panels inventing their own project path logic.
Runtime using different manifest assumptions than the editor.
Packaging seeing a different project shape than launch preview.
Project open mutating files accidentally.
Missing engine compatibility checks.
Invalid project files reaching runtime.
```

## 5.3 Responsibilities

```text
Create project from template.
Open/validate project manifest.
Resolve project-relative paths deterministically.
Validate engine compatibility.
Validate scene/asset/script/package profile references.
Validate diagnostics output folders.
Emit project_validation_report.json.
Offer project doctor checks for missing folders/files.
```

## 5.4 Recommended Language

**C#**

Reason:

```text
Strong CLI ecosystem.
Good JSON/YAML handling.
Cross-platform.
Good fit for manifest validation.
Good fit for editor/tooling workflows.
Avoids unnecessary C++ build coupling.
```

## 5.5 External Libraries

Recommended:

```text
System.CommandLine
System.Text.Json
JsonSchema.Net or NJsonSchema
YamlDotNet
Spectre.Console
Microsoft.Extensions.Logging
```

Optional:

```text
Serilog
CliWrap
```

Avoid:

```text
heavy database dependencies
GUI dependencies
engine private header dependencies
```

## 5.6 CLI Examples

```bash
sagaproject create --name MultiplayerSandbox --template multiplayer-basic
sagaproject validate --project MultiplayerSandbox.sagaproj --out Build/Reports/project_validation_report.json
sagaproject doctor --project MultiplayerSandbox.sagaproj
sagaproject resolve --project MultiplayerSandbox.sagaproj --json
```

## 5.7 Inputs

```text
*.sagaproj
project templates
engine compatibility metadata
scene/package/script profile references
```

## 5.8 Outputs

```text
project_validation_report.json
project_resolution.json
project_doctor_report.json
```

## 5.9 Repo Strategy

Start in monorepo:

```text
Tools/SagaProjectKit/
```

Possible future separate repo:

```text
Only if project manifest/schema validation becomes engine-agnostic.
```

## 5.10 First MVP Proof

```text
Create MultiplayerSandbox project.
Validate project manifest.
Resolve script, asset, package, diagnostics, and launch folders.
Emit deterministic project_validation_report.json.
Project open validation does not mutate files.
```

---

# 6. SagaLaunchLab

## 6.1 Purpose

SagaLaunchLab owns local playable preview orchestration.

It starts and supervises local SagaServer/SagaRuntime processes or in-process preview hosts according to launch profiles.

## 6.2 What It Prevents

SagaLaunchLab prevents:

```text
Manual launch command chains.
Editor-specific process orchestration bugs.
Server/client preview behaving differently per developer machine.
Unclear crash ownership when local preview fails.
No deterministic launch report.
No clean shutdown evidence.
```

## 6.3 Responsibilities

```text
Read launch profiles.
Start local server.
Start one or more clients.
Pass package/project arguments.
Capture stdout/stderr.
Capture process exit codes.
Capture diagnostics report paths.
Enforce timeout.
Clean shutdown.
Emit launch_preview_report.json.
```

## 6.4 Recommended Language

**C#**

Reason:

```text
Good process orchestration.
Good cross-platform CLI.
Good JSON/reporting ecosystem.
Good integration with editor and Forge.
No need for low-level C++.
```

## 6.5 External Libraries

Recommended:

```text
System.CommandLine
System.Text.Json
Spectre.Console
CliWrap
Microsoft.Extensions.Logging
```

Optional:

```text
Serilog
Polly for retry/timeouts
```

Avoid:

```text
embedding engine runtime
direct UI dependency
platform-specific shell scripts as source of truth
```

## 6.6 CLI Examples

```bash
sagalaunch preview --project MultiplayerSandbox.sagaproj --profile local-2-client
sagalaunch server --project MultiplayerSandbox.sagaproj --profile local-server
sagalaunch client --project MultiplayerSandbox.sagaproj --count 2
sagalaunch report --last
```

## 6.7 Inputs

```text
project manifest
launch profile
package manifest path
server/runtime executable paths
diagnostics output directory
```

## 6.8 Outputs

```text
launch_preview_report.json
process logs
diagnostics path summary
clean shutdown report
```

## 6.9 Repo Strategy

Keep in monorepo.

Reason:

```text
It is tightly coupled to Saga executable names, launch arguments, package layout, and runtime/server preview behavior.
```

## 6.10 First MVP Proof

```text
Start local server.
Start one local client.
Collect diagnostics.
Shut down cleanly.
Emit launch_preview_report.json.
```

Later:

```text
2-client preview.
Failure diagnostics.
Editor button integration.
```

---

# 7. SagaPackager / SagaPublishGate

## 7.1 Purpose

SagaPackager builds and validates packaged game outputs.

SagaPublishGate determines whether a project is ready for a given publish profile.

These can be one tool initially:

```text
sagapack
```

with publish subcommands:

```text
sagapack publish-check
```

or two commands:

```text
sagapack
sagapublish
```

## 7.2 What It Prevents

It prevents:

```text
Editor-only projects that cannot run outside the editor.
Missing assets passing publish.
Stale script bindings passing publish.
Invalid package manifests.
Manual packaging drift.
No reproducible packaged launch proof.
Overclaiming MVP/package readiness.
```

## 7.3 Responsibilities

```text
Validate package profiles.
Validate package manifest.
Validate asset manifest references.
Validate script/runtime metadata presence.
Run package smoke test.
Generate launch command.
Generate publish_report.json.
Block publish on critical diagnostics.
```

## 7.4 Recommended Language

**C#** for CLI and validation orchestration.

Use **C++** only for runtime-side package consumers/tests already inside engine/runtime.

Reason:

```text
Packaging is manifest/report heavy.
C# is efficient for CLI, JSON, filesystem, process orchestration.
Runtime package loaders remain C++ because runtime is C++.
```

## 7.5 External Libraries

Recommended C#:

```text
System.CommandLine
System.Text.Json
JsonSchema.Net or NJsonSchema
Spectre.Console
CliWrap
Microsoft.Extensions.Logging
SharpZipLib or System.IO.Compression
```

Optional:

```text
Serilog
Checksum libraries from .NET cryptography APIs
```

Recommended C++ runtime/test side:

```text
nlohmann_json
GoogleTest
```

## 7.6 CLI Examples

```bash
sagapack build --project MultiplayerSandbox.sagaproj --profile local-demo
sagapack validate --package Build/Packages/MultiplayerSandbox
sagapack smoke --package Build/Packages/MultiplayerSandbox
sagapublish check --project MultiplayerSandbox.sagaproj --profile demo --report Build/Reports/publish_report.json
```

## 7.7 Inputs

```text
project manifest
package profile
asset manifest
asset identity manifest
script metadata
runtime bindings
diagnostics reports
launch profile
```

## 7.8 Outputs

```text
package_report.json
publish_report.json
package smoke report
launch command
package folder/archive
```

## 7.9 Repo Strategy

Start in monorepo.

Possible future separation:

```text
Package schema validators may become separate library.
Saga-specific staging remains monorepo.
```

## 7.10 First MVP Proof

```text
Validate MultiplayerSandbox package.
Detect missing asset.
Detect missing script metadata.
Generate publish_report.json.
Run packaged runtime smoke proof.
```

---

# 8. SagaProbe

## 8.1 Purpose

SagaProbe reads and summarizes diagnostics, crash, stress, lifetime, health, and publish reports.

It exists because report generation without report consumption becomes useless.

## 8.2 What It Prevents

SagaProbe prevents:

```text
Diagnostics reports becoming JSON graveyards.
Crash reports being unreadable.
Stress results being hard to compare.
Lifetime leaks being buried.
Health snapshots not influencing product decisions.
```

## 8.3 Responsibilities

```text
Read diagnostic reports.
Summarize health metrics.
Summarize crash context.
Summarize lifetime leaks.
Compare two runs.
Print human-readable CLI summary.
Emit normalized diagnostics_summary.json.
Optionally provide GUI later.
```

## 8.4 Recommended Language

Initial CLI:

**C#**

Optional future GUI:

**C++/Qt** if integrated with Saga Editor, or **C# Avalonia** if separate cross-platform GUI is desired.

Recommended decision:

```text
V1: C# CLI only.
Do not build GUI first.
```

## 8.5 External Libraries

C# CLI:

```text
System.CommandLine
System.Text.Json
Spectre.Console
Microsoft.Extensions.Logging
JsonSchema.Net or NJsonSchema
```

Optional chart/report export:

```text
ScottPlot for generated charts
QuestPDF for PDF summaries, if needed later
```

C++/Qt future GUI:

```text
Qt
nlohmann_json
spdlog
```

Avoid early:

```text
large web dashboard stack
database storage
remote telemetry service
```

## 8.6 CLI Examples

```bash
sagaprobe report Build/Reports/runtime_diagnostics.json
sagaprobe crash diagnostics/reports/crash_2026_05_27.json
sagaprobe lifetime diagnostics/reports/lifetime_leaks.json
sagaprobe compare run_a.json run_b.json
sagaprobe summarize --reports Build/Reports --out Build/Reports/diagnostics_summary.json
```

## 8.7 Inputs

```text
diagnostic reports
crash reports
health snapshots
lifetime leak reports
stress reports
publish reports
```

## 8.8 Outputs

```text
diagnostics_summary.json
human-readable console summary
optional markdown summary
```

## 8.9 Repo Strategy

Start in monorepo.

Possible future separate repo:

```text
If diagnostics report format becomes stable and useful outside Saga.
```

## 8.10 First MVP Proof

```text
Read one health/diagnostic report.
Print critical warnings.
Emit diagnostics_summary.json.
Fail if report contains critical diagnostics when --strict is passed.
```

---

# 9. SagaStressArena

## 9.1 Purpose

SagaStressArena is a server/runtime stress and soak test harness.

It exercises server lifecycle, fake clients, disconnect/reconnect behavior, input movement, and diagnostics reporting.

## 9.2 What It Prevents

SagaStressArena prevents:

```text
Server only working on happy path.
Session leaks.
Disconnected players remaining registered.
Packet queues growing silently.
Long-running server degradation being invisible.
No stress evidence for diagnostics claims.
```

## 9.3 Responsibilities

```text
Start direct or process-based server harness.
Spawn fake/bot clients.
Send movement/input.
Disconnect/reconnect clients.
Run smoke/mini-soak/heavy profiles.
Emit stress report.
Emit diagnostics reports.
Support deterministic seeds.
```

## 9.4 Recommended Language

**C++**

Reason:

```text
Needs to exercise Saga C++ runtime/server systems.
Needs CTest/GoogleTest integration.
May need direct access to test-only server hooks.
Performance and deterministic behavior matter.
```

## 9.5 External Libraries

Recommended:

```text
GoogleTest
nlohmann_json
CLI11 or cxxopts
Boost.Asio where needed
spdlog or existing Saga logging
```

Optional:

```text
fmt
date/tz if not already covered by standard library
```

Avoid:

```text
large scripting harness first
Python as core stress runner
GUI
```

## 9.6 CLI Examples

```bash
sagastress run --scenario smoke --clients 10 --duration 30s
sagastress run --scenario mini_soak --clients 50 --duration 10m
sagastress report --last
```

## 9.7 Inputs

```text
stress scenario
server launch/direct harness config
client count
duration
seed
chaos profile optional
```

## 9.8 Outputs

```text
stress_report.json
operational snapshot
reliability report
lifetime leak report
```

## 9.9 Repo Strategy

Keep in monorepo.

Reason:

```text
It is not a generic stress tool.
It is a SagaServer/SagaRuntime diagnostics harness.
```

## 9.10 First MVP Proof

```text
Run direct local server diagnostics harness.
Simulate small client/input lifecycle.
Emit stress_report.json.
Verify no lifetime leak in smoke scenario.
```

---

# 10. SagaChaosLab

## 10.1 Purpose

SagaChaosLab defines and runs network chaos scenarios for Saga networking and server tests.

It should simulate:

```text
packet loss
latency
jitter
reconnect storm
duplicate packets
out-of-order packets where relevant
```

## 10.2 What It Prevents

SagaChaosLab prevents:

```text
Networking only working on perfect localhost.
Reconnect/session cleanup bugs staying hidden.
Replication assuming ideal packet order.
Timeout behavior not being tested.
Network chaos claims being unverified.
```

## 10.3 Responsibilities

```text
Define deterministic chaos profiles.
Integrate with SagaNet chaos layer.
Integrate with SagaStressArena scenarios.
Emit chaos report.
Record packet policy statistics.
```

## 10.4 Recommended Language

**C++** for chaos layer/policy integration.

Optional **C#** wrapper for profile generation/CLI orchestration if useful later.

Recommended V1:

```text
C++ only, integrated with SagaStressArena.
```

## 10.5 External Libraries

C++:

```text
nlohmann_json
GoogleTest
CLI11 or cxxopts
Boost.Asio if network simulation needs it
```

Optional:

```text
pcg_random or standard <random> with fixed seed
```

Avoid:

```text
external network proxies first
complex distributed chaos platform
Kubernetes-style chaos tooling
```

## 10.6 CLI Examples

```bash
sagachaos validate --profile chaos_profiles/loss_5_latency_100.json
sagastress run --scenario reconnect_storm --chaos chaos_profiles/reconnect_storm.json
```

## 10.7 Inputs

```text
chaos profile JSON
seed
stress scenario
network/session config
```

## 10.8 Outputs

```text
chaos_report.json
packet policy summary
reconnect summary
```

## 10.9 Repo Strategy

Keep in monorepo.

Reason:

```text
Chaos profiles are meaningful only with SagaNet/SagaServer semantics.
```

## 10.10 First MVP Proof

```text
Apply deterministic packet loss profile in local stress scenario.
Emit packet drop/latency statistics.
Verify server does not crash.
```

Do not start this before a stable local server/client path exists.

---

# 11. SagaStateCheck

## 11.1 Purpose

SagaStateCheck validates authoritative server state, replication consistency, and desync reports.

It should not be built too early. It becomes useful once replication and state snapshots exist.

## 11.2 What It Prevents

SagaStateCheck prevents:

```text
Silent client/server desync.
Replication mismatch going unnoticed.
Invalid authority ownership changes.
Entity state corruption.
Checksum reports that nobody reads.
```

## 11.3 Responsibilities

```text
Read server/client state snapshots.
Compute entity/world checksums.
Compare replication snapshots.
Emit desync report.
Validate authority ownership.
Analyze state validation diagnostics.
```

## 11.4 Recommended Language

**C++**

Reason:

```text
State validation is tightly coupled to server/runtime data structures.
C++ tests can directly validate engine/server behavior.
```

Optional C# CLI summarizer later if reports become complex.

## 11.5 External Libraries

C++:

```text
nlohmann_json
GoogleTest
CLI11 or cxxopts
xxHash or equivalent checksum library if acceptable
```

If avoiding external checksum dependency initially:

```text
use deterministic internal hash utility
```

## 11.6 CLI Examples

```bash
sagastatecheck compare --server server_snapshot.json --client client_snapshot.json
sagastatecheck validate --report replication_snapshot.json
```

## 11.7 Inputs

```text
server state snapshot
client state snapshot
replication snapshot
authority ownership report
```

## 11.8 Outputs

```text
desync_report.json
state_validation_report.json
checksum_summary.json
```

## 11.9 Repo Strategy

Keep in monorepo.

Reason:

```text
State semantics are Saga-specific.
```

## 11.10 First MVP Proof

```text
Compare two small deterministic entity snapshots.
Detect one changed component value.
Emit desync_report.json.
```

Do not implement before replication/state snapshot format exists.

---

# 12. SagaViewKit

## 12.1 Purpose

SagaViewKit defines and validates editor view capabilities and abstraction levels.

It supports the product idea that different users can use different views over the same project:

```text
Simple Blocks View
High-Level Behavior View
Pro Graph View
C# Source View
Scene View
Asset View
QA View
Diagnostics View
Team Room
```

## 12.2 What It Prevents

SagaViewKit prevents:

```text
Simple View lying about advanced behavior.
Each editor panel inventing capabilities.
Pro View and Simple View drifting into separate truths.
Editor customization mutating project truth.
Collaboration not knowing which view can edit which artifact.
```

## 12.3 Responsibilities

```text
Define view capability manifest schema.
Validate view capability manifests.
Define abstraction levels.
Define display/edit permissions per view.
Validate "simple view must not lie" constraints.
Emit view compatibility report.
Support collaboration/policy systems with view metadata.
```

## 12.4 Recommended Language

**C#**

Reason:

```text
Manifest/schema validation.
Editor/tooling integration.
Good JSON/YAML libraries.
Does not need C++ runtime internals.
```

## 12.5 External Libraries

Recommended:

```text
System.CommandLine
System.Text.Json
JsonSchema.Net or NJsonSchema
YamlDotNet
Spectre.Console
Microsoft.Extensions.Logging
```

## 12.6 CLI Examples

```bash
sagaview validate --manifest Editor/Views/simple_blocks.view.json
sagaview compare --simple simple_blocks.view.json --pro pro_graph.view.json
sagaview report --views Editor/Views --out Build/Reports/view_capability_report.json
```

## 12.7 Inputs

```text
view capability manifests
behavior projection metadata
project policy metadata optional
```

## 12.8 Outputs

```text
view_capability_report.json
view_compatibility_report.json
```

## 12.9 Repo Strategy

Start in monorepo.

Possible future separation:

```text
Only if view capability schema becomes independent and reusable.
```

## 12.10 First MVP Proof

```text
Validate Simple Blocks View manifest.
Validate Pro Graph View manifest.
Detect that Simple View cannot edit Advanced C# nodes.
Emit view_capability_report.json.
```

---

# 13. SagaWorkspaceHub

## 13.1 Purpose

SagaWorkspaceHub is the collaboration session service.

It owns:

```text
sessions
presence
locks
semantic transactions
artifact versions
Team Room event feed
```

It should not be a remote desktop system. It should coordinate personal editor views through the shared Saga Project Model.

## 13.2 What It Prevents

SagaWorkspaceHub prevents:

```text
Collaboration becoming shared-screen control.
Everyone being forced into one user's editor layout.
Presence being stored as project history.
Semantic project changes becoming UI gesture sync.
Locks and transactions becoming inconsistent.
```

## 13.3 Responsibilities

```text
Manage workspace sessions.
Track online users.
Track presence records.
Track active artifact.
Acquire/release locks.
Append semantic transactions.
Maintain artifact versions.
Broadcast allowed events.
Emit workspace_session_report.json.
```

## 13.4 Recommended Language

Two realistic choices:

### Option A — C#

Recommended for early implementation.

Reason:

```text
Matches other Saga product tools.
Fast to build.
Good JSON and WebSocket libraries.
Easier integration with editor/toolchain.
```

### Option B — Rust

Recommended for long-term service if security/concurrency becomes serious.

Reason:

```text
Strong async service ecosystem.
Memory safety.
Good for long-running workspace server.
Good for future enterprise service boundary.
```

Recommended path:

```text
V1: C# in-process/local service.
V2/V3: consider Rust if it becomes a real networked workspace server.
```

## 13.5 External Libraries

C# V1:

```text
System.CommandLine
System.Text.Json
Microsoft.Extensions.Hosting
Microsoft.Extensions.Logging
ASP.NET Core minimal APIs
WebSockets
SignalR optional
LiteDB optional for local persistence
```

Rust V2:

```text
tokio
axum
serde
serde_json
tracing
uuid
sqlx optional
sled optional
jsonschema
```

Avoid early:

```text
full cloud stack
Kubernetes
Kafka
distributed consensus
CRDT framework
```

## 13.6 CLI Examples

```bash
sagaworkspace serve --project Anhos.sagaproj --mode local-team
sagaworkspace inspect --workspace workspace_anhos
sagaworkspace replay --transactions workspace_transactions.json
```

## 13.7 Inputs

```text
project manifest
workspace config
user/session config
view capability metadata
optional policy metadata
```

## 13.8 Outputs

```text
workspace_session_report.json
presence.events.jsonl
transaction.events.jsonl
lock.events.jsonl
```

## 13.9 Repo Strategy

Start in monorepo.

Possible future separate repo:

```text
If it becomes a real workspace server with stable protocol.
```

## 13.10 First MVP Proof

```text
3 users connect to local workspace.
Presence visible.
Behavior artifact lock acquired.
Second user sees read-only lock state.
Simple transaction appended and replayed.
```

Do not build this before basic project model and at least one cross-view behavior artifact proof exists.

---

# 14. SagaPolicyKit

## 14.1 Purpose

SagaPolicyKit owns permissions, project slices, source visibility, redaction rules, and policy diagnostics.

It is primarily a future enterprise-enabling tool. It should not block early MVP.

## 14.2 What It Prevents

SagaPolicyKit prevents:

```text
UI hiding being mistaken for security.
Unauthorized clients receiving full project data.
External contractors seeing source code.
Team Room leaking restricted artifact names.
Publish policy being scattered across tools.
```

## 14.3 Responsibilities

```text
Define roles.
Define permissions.
Define project slice rules.
Define source visibility levels.
Redact restricted artifact references.
Validate policy documents.
Emit policy_check_report.json.
Provide policy decisions to WorkspaceHub and PublishGate.
```

## 14.4 Recommended Language

**Rust** for long-term service-quality policy engine.

Alternative **C#** for early policy manifest validator.

Recommended path:

```text
V1: C# policy manifest validator.
V2: Rust service/library if permissioned project slices become real.
```

Reason:

```text
Policy/security code benefits from memory safety and strict boundaries.
But early Saga needs fast iteration more than a hardened service.
```

## 14.5 External Libraries

C# V1:

```text
System.CommandLine
System.Text.Json
JsonSchema.Net or NJsonSchema
YamlDotNet
Spectre.Console
```

Rust V2:

```text
serde
serde_json
schemars
jsonschema
thiserror
tracing
uuid
```

Optional later:

```text
OPA/Rego integration only if truly needed
```

Avoid early:

```text
complex enterprise IAM
SSO
cloud org service
cryptographic audit hardening
```

## 14.6 CLI Examples

```bash
sagapolicy validate --policy workspace_policy.json
sagapolicy check --user user_designer --operation EditCSharp --artifact Scripts/DoorLogic.cs
sagapolicy slice --user contractor_01 --project Anhos.sagaproj --out Build/Slices/contractor_01
```

## 14.7 Inputs

```text
workspace policy
user roles
project manifest
artifact graph
source visibility metadata
view capability metadata
```

## 14.8 Outputs

```text
policy_check_report.json
project_slice_manifest.json
redaction_report.json
```

## 14.9 Repo Strategy

Start in monorepo.

Possible future separate repo:

```text
If project slicing/security policy becomes a standalone workspace service.
```

## 14.10 First MVP Proof

```text
User A can view Map_A.
User A cannot view restricted script.
Restricted artifact name is redacted.
Policy check emits deterministic denial diagnostic.
```

Do not build before collaboration core is useful.

---

# 15. SagaDocGuard

## 15.1 Purpose

SagaDocGuard validates documentation claims against repository evidence.

It is a claim/evidence gate, not a grammar checker.

## 15.2 What It Prevents

SagaDocGuard prevents:

```text
Planning docs overclaiming implementation status.
Docs saying MVP complete without package proof.
Docs saying enterprise-ready without project slices/audit.
Docs saying full CTest passed without evidence.
AI-generated iteration notes inflating reality.
Non-claims disappearing from product documents.
```

## 15.3 Responsibilities

```text
Scan markdown docs.
Detect heavy claims.
Classify claim types.
Collect evidence reports.
Check required evidence.
Check required non-claims.
Warn about stale evidence.
Emit docguard_report.json.
Optionally fail Forge/CI gate.
```

## 15.4 Recommended Language

**C#**

Reason:

```text
Good CLI and JSON handling.
Typed rule models.
Good cross-platform support.
Fits Saga toolchain.
Easier than C++ for document/report processing.
```

Python is acceptable for a temporary prototype, but not recommended as the long-term core if this becomes a serious gate.

## 15.5 External Libraries

Recommended:

```text
System.CommandLine
System.Text.Json
YamlDotNet
Markdig
Spectre.Console
Microsoft.Extensions.Logging
JsonSchema.Net or NJsonSchema
```

Optional:

```text
Serilog
DiffPlex for text diff reporting
```

Avoid:

```text
LLM-dependent validation in V1
heavy NLP
web dependencies
```

## 15.6 CLI Examples

```bash
sagadocguard check --docs docs --reports Build/Reports --out Build/Reports/docguard_report.json
sagadocguard claims --docs docs
sagadocguard nonclaims --docs docs --strict
```

## 15.7 Inputs

```text
docs/**/*.md
Build/Reports/*.json
diagnostics/reports/*.json
test reports
publish reports
tool reports
claim rules YAML
non-claim rules YAML
```

## 15.8 Outputs

```text
docguard_report.json
docguard_summary.md
claim inventory
overclaim diagnostics
```

## 15.9 Repo Strategy

Start in monorepo.

Possible future separate repo:

```text
Yes, if claim/evidence validation becomes generic and stable.
```

## 15.10 First MVP Proof

```text
Detect "production-ready" claim.
Require configured evidence.
Fail if evidence is missing.
Verify required non-claims exist in MVP docs.
Emit docguard_report.json.
```

---

# 16. Tool Interaction Map

Recommended high-level flow:

```text
SagaProjectKit
    validates project
        ↓
SagaWeaver
    validates/project scripts and emits script artifacts
        ↓
SagaLaunchLab
    launches local preview using project/package profiles
        ↓
SagaProbe
    reads diagnostics from preview
        ↓
SagaPackager / SagaPublishGate
    validates package and publish readiness
        ↓
SagaDocGuard
    verifies docs do not overclaim evidence
```

Reliability branch:

```text
SagaStressArena
    ↓ uses optional
SagaChaosLab
    ↓ emits
stress/chaos/diagnostics reports
    ↓ consumed by
SagaProbe
    ↓ evidence consumed by
SagaDocGuard / PublishGate
```

Collaboration branch:

```text
SagaViewKit
    defines view capabilities
        ↓
SagaWorkspaceHub
    sessions/presence/locks/transactions
        ↓
SagaPolicyKit
    permissions/project slices/redaction
        ↓
SagaDocGuard
    evidence/claim validation
```

---

# 17. What Not To Build Early

Do not build early:

```text
cloud workspace platform
full enterprise policy server
remote streamed editor
complex CRDT/OT collaboration
web dashboard for everything
AI-based doc reasoning gate
plugin marketplace
distributed stress cloud
security certification tooling
commercial installer/publisher
```

The first goal is not a full platform.

The first goal is:

```text
Project validates.
Preview launches.
Diagnostics are readable.
Package proof works.
Docs do not overclaim.
```

---

# 19. Toolchain Success Criteria

The auxiliary toolchain is successful when Saga can prove:

```text
A project can be created and validated.
A local playable preview can be launched reproducibly.
Diagnostics can be read and summarized.
A package can be validated and smoke-tested.
Publish readiness can be checked.
Docs cannot claim more than evidence supports.
Stress/chaos/state tools provide reliability evidence when systems mature.
Collaboration tools coordinate personal editor views through shared project transactions.
Policy tools can eventually enforce project slices without UI-hiding security lies.
```

The toolchain is not successful if:

```text
There are many tools but no playable proof.
Tools duplicate validation logic.
Tools require private engine internals unnecessarily.
Reports exist but nobody reads them.
Each tool invents incompatible diagnostics.
Docs still overclaim.
```

---

# 20. Final Recommendation

SagaEngine should not try to solve all product risks inside the engine runtime or editor.

The recommended auxiliary toolchain, excluding SagaWeaver, is:

```text
SagaProjectKit
    project truth

SagaLaunchLab
    playable preview proof

SagaPackager / SagaPublishGate
    package/publish proof

SagaProbe
    diagnostics visibility

SagaStressArena
    runtime reliability stress evidence

SagaChaosLab
    hostile network condition evidence

SagaStateCheck
    server state/replication correctness evidence

SagaViewKit
    editor abstraction/view capability truth

SagaWorkspaceHub
    collaboration session/transaction truth

SagaPolicyKit
    permission/project-slice/security truth

SagaDocGuard
    documentation claim/evidence truth
```

The strategic rule:

```text
A tool is justified only if it turns an important product claim into testable evidence.
```

The engineering rule:

```text
Tools produce versioned artifacts. Engine/editor/runtime consume artifacts.
```

The product rule:

```text
Do not build more tools than needed to prove the next vertical slice.
```

The first serious vertical chain should be:

```text
SagaProjectKit
    → SagaWeaver
    → SagaLaunchLab
    → SagaProbe
    → SagaPackager / SagaPublishGate
    → SagaDocGuard
```

SagaWeaver is omitted from this document by design, but it remains the C# visual-authoring core. The tools in this document surround it and make SagaEngine a reproducible product ecosystem rather than a loose collection of engine modules.
