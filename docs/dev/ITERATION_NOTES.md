# Iteration Notes

> Status: Active development note
> Current iteration: `0.0.8-dev.7`
> Purpose: Track the current implementation batch before roadmap, architecture, diagnostics, manifest, and test documents are updated.

---

## 0. Purpose

This document records the current development iteration.

It is not a changelog.

It is not a release note.

It is not a roadmap.

It is not permanent architecture truth.

It is a temporary working note used to capture what changed during the current batch.

Permanent decisions belong in:

```txt
docs/*_ROADMAP.md
docs/DependencyGraph.md
docs/SCHEMA.md
```

This file may be rewritten for each iteration.

---

## 1. Iteration

```txt
Version: 0.0.8-dev.7
Status: Draft
Date: 2026-05-20
```

Note:

```txt
0.0.8-dev.6 completed and committed before this iteration started.
```

Short summary:

```txt
Added SagaSync internal multirepo dashboard foundation.
Added SagaScript roadmap contract for C# scripting, block integration, authority, capability, and package boundaries.
Added SagaShared SagaScript contract descriptors and focused contract tests.
Added standalone SagaScript toolchain foundation for C# source validation, binding inspection, manifest emission, and diagnostics.
Added Forge generic external diagnostics gate foundation without SagaEngine/SagaScript implementation coupling.
```

---

## 2. What Was Added

```txt
- SagaSync thin entrypoint with non-GUI smoke mode.
- SagaSync core services for git status, export manifest/state parsing, export health, verification profiles, session run history, command execution, commit queue suggestions, and commit plan preview.
- SagaSync PySide6 UI package for dashboard widgets.
- SagaSync README and focused core tests.
- SagaTools registry generation entry for `sagasync`.
- SagaScript roadmap defining the C# scripting ecosystem, block/text script bridge, authority metadata, capability model, artifacts, diagnostics, package gates, and ownership boundaries.
- SagaShared SagaScript data-only contracts for script ids, artifact references, authority descriptors, capability descriptors, diagnostics payloads, generated-code origin metadata, binding descriptors, and script manifests.
- SagaScript shared contract unit tests covering binding authority/capability metadata, generated origin metadata, artifact manifests, capability manifests, and diagnostic summaries.
- Standalone `Tools/SagaScript` .NET 8 CLI foundation with Roslyn parsing isolated to the tool boundary.
- `sagascript validate` for C# source or directory validation with JSON-compatible diagnostic reporting.
- `sagascript inspect-bindings` for source-level binding metadata inspection.
- `sagascript emit-manifests` for initial `script_bindings.json`, `script_capabilities.json`, and `sagascript_diagnostics.json` output.
- SagaScript v1 source metadata extraction for block callable methods, block names/categories, authority metadata, side-effect metadata, requested capabilities, script ids, generated origin metadata, source locations, and source hashes.
- SagaScript v1 validation diagnostics for missing authority, unsafe authority/side-effect combinations, unsupported parameter and return types, duplicate script ids, duplicate binding identities, unrecognized capabilities, and blocked `EngineInternal` capability requests outside engine-owned scripts.
- Focused SagaScript CLI tests covering valid manifest emission, blocking diagnostics, warnings, JSON output, and SagaShared dependency boundary scanning.
- Thin `sagascript` and `sagascript.cmd` launch wrappers.
- SagaScript tool README documenting commands, supported attributes, outputs, and non-goals.
- Forge generic `gate run` command for invoking an external tool through a command boundary and failing on a configured diagnostics boolean.
- Forge `ExternalGateAdapter` for external-tool execution and diagnostics gate evaluation without owning tool internals.
- Forge fake external gate tool and focused command tests covering success, blocking diagnostics, missing diagnostics, pass-through arguments, custom blocker keys, and explain mode.
- `sagascript compile` for validated C# script assembly emission, runtimeconfig emission, script artifact manifests, generic script artifact manifests, and diagnostics.
- Product-side SagaScript gate wiring that invokes `sagascript compile` through Forge's generic external gate.
- Package staging support for reading `script_artifacts.json` and writing client/server-specific script artifact manifests.
- Focused SagaScript compile test coverage and product package staging coverage for server-only script artifact placement.
- Deterministic SagaScript `.NET 10` SDK availability diagnostics for `sagascript compile`.
- Focused missing-SDK toolchain check test for the SagaScript launcher.
- Engine-owned SagaScript host abstraction for editor-independent package load, class instance creation, lifecycle invocation, opaque package/instance handles, and host diagnostics.
- `ScriptLifecycleService` for deterministic `OnCreate`, `OnStart`, `OnUpdate(deltaTime)`, and `OnDestroy` dispatch through `ISagaScriptHost` without CoreCLR execution.
- Focused `MockScriptHost` lifecycle tests covering successful package load, missing script class, instance creation, lifecycle order, lifecycle failure diagnostics, invalid instance handles, and forbidden dependency tokens.
- `ScriptPackageValidator` for manifest/file/destination validation before script package load reaches a host.
- Focused script package validation tests covering valid package load, missing script artifact manifest, missing `.scripts.dll`, escaping artifact paths, and package destination mismatch.
- Script artifact metadata policy validation for target framework, hash metadata, authority metadata, capability arrays, and authority/destination compatibility.
- Focused script metadata policy tests covering unsupported target framework, invalid hash metadata, missing authority, authority/destination mismatch, malformed capabilities, and shared destination acceptance.
- Script artifact SHA-256 recomputation for referenced `.scripts.dll` files before host load.
- Deterministic `Script.Host.ArtifactHashMismatch` diagnostics for assembly content that no longer matches `script_artifacts.json`.
- Capability manifest consistency validation between `script_artifacts.json` and sibling `script_capabilities.json` before host load.
- Deterministic `Script.Host.CapabilityManifestMissing`, `Script.Host.CapabilityManifestInvalid`, and `Script.Host.CapabilityManifestMismatch` diagnostics for missing, malformed, or inconsistent capability manifests.
- Strict capability grant policy validation that blocks host load unless each script artifact's `requestedCapabilities` and `grantedCapabilities` are an exact set match.
- Deterministic `Script.Host.CapabilityGrantMissing` and `Script.Host.CapabilityGrantUnexpected` diagnostics for missing requested grants and extra unrequested grants.
- Managed `SagaScript.RuntimeBridge` assembly with explicit `SagaScript` base class contract and native-callable lifecycle bridge entrypoints.
- `CSharpScriptHost` as the first hostfxr/nethost-backed `ISagaScriptHost` implementation for loading `.scripts.dll` assemblies and invoking managed lifecycle methods.
- Focused `CSharpScriptHostTests` covering managed package load, class resolution, instance creation, lifecycle invocation, delta forwarding, invalid class, missing class, managed exception, missing/invalid runtimeconfig, and missing assembly diagnostics.
- Managed `ScriptContext` V1 with `Context.World`, `Context.Self`, `Context.Log`, `ScriptEntity`, and `ScriptVector3`.
- Native-to-managed callback bridge V1 for managed logging, entity creation, and simple position read/write without exposing raw native pointers.
- `WorldStateScriptWorld` production adapter for mapping script-facing opaque entity handles to `Simulation::WorldState` entities and `TransformComponent` position state.
- Runtime capability gates for `Gameplay.World.CreateEntity`, `Gameplay.Transform.Read`, and `Gameplay.Transform.Write`.
- Focused ScriptContext CSharp host tests covering context availability, log observation, entity creation, self entity mutation, position get/set, delta-time position mutation, invalid entity handles, and missing capability diagnostics.
- Focused editorless `CSharpGameplayProofTests` target that compiles real SagaScript C# source through `sagascript compile`, grants generated manifest capabilities in-place, loads through `ScriptLifecycleService` plus `CSharpScriptHost`, and verifies `Context.Self`, `Context.World.TryCreateEntity`, `Context.Log`, and capability grant denial against `WorldStateScriptWorld`.
```

---

## 3. What Was Changed

```txt
- docs/dev/ITERATION_NOTES.md started for 0.0.8-dev.7.
- docs/roadmaps/TOOLS_ROADMAP.md updated with SagaSync foundation notes.
- docs/dev/ITERATION_NOTES.md updated for the SagaScript roadmap-only implementation.
- docs/dev/ITERATION_NOTES.md updated for the SagaShared SagaScript contract slice.
- `.gitignore` now ignores `obj/` so local .NET restore/build intermediates are not staged.
- docs/dev/ITERATION_NOTES.md updated for the SagaScript toolchain and manifest gate foundation.
- Tools/Forge remains standalone by exposing a generic external diagnostics gate instead of a SagaEngine- or SagaScript-specific command.
- Tools/Forge CMake project now builds the generic gate adapter and standalone Forge gate tests.
- Tools/SagaScript now targets `.NET 10` for the C# scripting toolchain baseline.
- SagaPackageStaging now splits script artifact manifest refs by package destination instead of placing server-only scripts in client package manifests.
- `sagascript compile` now checks for a .NET 10 SDK before invoking `dotnet run` and emits `Script.Toolchain.DotNetSdkMissing` when the SDK is unavailable.
- SagaScript compile syntax trees now use UTF-8 `SourceText` so deterministic PDB emission succeeds under Roslyn instead of failing with `CS8055`.
- Saga package staging tests now have a focused `SagaPackageStagingTests` target that avoids the full Saga product shell and engine link graph.
- Saga script lifecycle tests now have a focused `SagaScriptLifecycleTests` target that avoids the full runtime/server/editor/product test graph.
- `ScriptLifecycleService::LoadPackage` now rejects invalid script packages before dispatching to `ISagaScriptHost::LoadPackage`.
- `ScriptPackageValidator` now rejects unsupported script target frameworks, invalid artifact hash metadata, missing authority metadata, malformed capability metadata, and authority metadata that conflicts with the requested package destination.
- `ScriptPackageValidator` now parses `sha256:<64 hex>` artifact hashes and compares them against OpenSSL EVP SHA-256 digests of referenced `.scripts.dll` files.
- `SagaScriptLifecycleTests` links `OpenSSL::Crypto` for focused hash-validation coverage without broadening the editor/product/runtime test graph.
- `ScriptPackageValidator` now requires `script_capabilities.json` next to `script_artifacts.json` and verifies script authority, destination, requested capabilities, and granted capabilities agree with each script artifact entry.
- `ScriptPackageValidator` now enforces least-privilege capability grants after artifact and capability-manifest validation succeeds.
- `ScriptPackageValidator` now treats artifact capability arrays with non-string elements as `Script.Host.InvalidCapabilityMetadata` instead of allowing them through grant-policy evaluation.
- `SagaScriptLifecycleTests` fixtures now emit matching `script_capabilities.json` by default and corrupt it only in targeted manifest-consistency tests.
- `shell.nix` now includes `dotnet-sdk_10`, exports `DOTNET_ROOT`, and exposes .NET 10 host/runtime libraries for SagaScript and CSharpScriptHost work.
- CMake now probes the .NET 10 host pack for `nethost.h`, `hostfxr.h`, `coreclr_delegates.h`, and `libnethost`.
- `Tools/SagaScript` compile now requires executable script classes to derive from `SagaScript` and references `SagaScript.RuntimeBridge.dll` instead of relying only on generated attribute shims.
- `SagaEngine` now links the native nethost library and includes the isolated CSharp hosting source while `ScriptLifecycleService` remains backend-agnostic.
- `ISagaScriptHost` now accepts request-based instance creation for explicit script id and self-entity context while keeping the lifecycle-only overload for existing tests and callers.
- `CSharpScriptHost` now parses granted capabilities per script id at package load and enforces them at managed engine API callback time.
- `SagaScript.RuntimeBridge` now assigns `SagaScript.Context` before lifecycle methods run and exposes only `Try*` engine operations to managed scripts.
- `SagaTests.cmake` now keeps the editorless C# gameplay proof in a separate focused CTest target instead of folding it into the broad `SagaUnitTests` graph.
```

---

## 4. What Was Removed

```txt
- The aborted Forge-side SagaScript-specific adapter/command shape was removed before landing.
- No shipped server/editor/Prism integration was removed.
- Duck-typed lifecycle-only C# script execution was intentionally not added for V1.
- No broad all-access managed engine API was added; V1 is limited to log, self, entity create, and simple position get/set.
```

---

## 5. Files Changed

```txt
- docs/dev/ITERATION_NOTES.md
- docs/roadmaps/SAGA_SCRIPTING_ROADMAP.md
- docs/roadmaps/TOOLS_ROADMAP.md
- Tools/SagaSync/README.md
- Tools/SagaSync/sagasync
- Tools/SagaSync/sagasync.cmd
- Tools/SagaSync/sagasync.py
- Tools/SagaSync/shell.nix
- Tools/SagaSync/core/__init__.py
- Tools/SagaSync/core/commands.py
- Tools/SagaSync/core/commit_plan.py
- Tools/SagaSync/core/commit_queue.py
- Tools/SagaSync/core/export_status.py
- Tools/SagaSync/core/export_health.py
- Tools/SagaSync/core/git_status.py
- Tools/SagaSync/core/actions.py
- Tools/SagaSync/core/verification.py
- Tools/SagaSync/core/run_history.py
- Tools/SagaSync/core/smoke.py
- Tools/SagaSync/ui/__init__.py
- Tools/SagaSync/ui/app.py
- Tools/SagaSync/ui/main_window.py
- Tools/SagaSync/tests/test_sagasync_core.py
- Tools/SagaTools/setup.py
- Tools/Forge/CMakeLists.txt
- Tools/Forge/include/Forge/Adapters/ExternalGateAdapter.hpp
- Tools/Forge/src/Adapters/ExternalGateAdapter.cpp
- Tools/Forge/src/main.cpp
- Tools/Forge/tests/FakeGateTool.cpp
- Tools/Forge/tests/ToolGateTests.cpp
- Tools/SagaScript/README.md
- Tools/SagaScript/SagaScript.csproj
- Tools/SagaScript/sagascript
- Tools/SagaScript/sagascript.cmd
- Tools/SagaScript/src/ManifestWriter.cs
- Tools/SagaScript/src/Models.cs
- Tools/SagaScript/src/Program.cs
- Tools/SagaScript/src/SagaScriptCompiler.cs
- Tools/SagaScript/src/SagaScriptAnalyzer.cs
- Tools/SagaScript/src/SourceDiscovery.cs
- Tools/SagaScript/tests/test_sagascript_cli.py
- Tools/SagaScript/tests/test_sagascript_toolchain_check.py
- .gitignore
- Shared/include/SagaShared/Scripting/GeneratedCodeOriginDescriptor.hpp
- Shared/include/SagaShared/Scripting/ScriptArtifactRef.hpp
- Shared/include/SagaShared/Scripting/ScriptAuthorityDescriptor.hpp
- Shared/include/SagaShared/Scripting/ScriptBindingDescriptor.hpp
- Shared/include/SagaShared/Scripting/ScriptCapabilityDescriptor.hpp
- Shared/include/SagaShared/Scripting/ScriptDiagnosticPayload.hpp
- Shared/include/SagaShared/Scripting/ScriptId.hpp
- Shared/include/SagaShared/Scripting/ScriptManifest.hpp
- Tests/Unit/Shared/SagaScriptContractsTests.cpp
- Apps/Saga/SagaScriptGate.h
- Apps/Saga/SagaScriptGate.cpp
- Apps/Saga/SagaPackageStaging.cpp
- Apps/Saga/SagaSessionModel.h
- Tests/Unit/Saga/SagaProductTests.cpp
- Tests/Unit/Saga/SagaPackageStagingTests.cpp
- Engine/Public/SagaEngine/Scripting/ISagaScriptHost.hpp
- Engine/Public/SagaEngine/Scripting/CSharpScriptHost.hpp
- Engine/Public/SagaEngine/Scripting/ScriptLifecycleService.hpp
- Engine/Public/SagaEngine/Scripting/ScriptPackageValidator.hpp
- Engine/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.csproj
- Engine/Managed/SagaScript.RuntimeBridge/SagaScript.cs
- Engine/Managed/SagaScript.RuntimeBridge/NativeBridge.cs
- Engine/Private/SagaEngine/Scripting/CSharpScriptHost.cpp
- Engine/Private/SagaEngine/Scripting/ScriptLifecycleService.cpp
- Engine/Private/SagaEngine/Scripting/ScriptPackageValidator.cpp
- Tests/Fixtures/CSharpScriptHost/CSharpScriptHostFixture.csproj
- Tests/Fixtures/CSharpScriptHost/TestScripts.cs
- Tests/Unit/Runtime/CSharpGameplayProofTests.cpp
- Tests/Unit/Runtime/CSharpScriptHostTests.cpp
- Tests/Unit/Runtime/ScriptLifecycleServiceTests.cpp
- shell.nix
- cmake/modules/SagaThirdparty.cmake
- cmake/modules/SagaTargets.cmake
- cmake/modules/SagaTests.cmake
```

---

## 6. Ownership / Boundary Notes

```txt
Allowed:
- SagaSync may orchestrate developer workflow visibility across SagaEngine and tool mirror exports.
- SagaScript roadmap may define scripting contracts and ownership boundaries before implementation begins.
- SagaShared may own neutral SagaScript ids, descriptors, artifact refs, manifest contracts, and diagnostics payload wrappers.
- Tools/SagaScript may own C# source validation, source-level metadata extraction, manifest emission, and diagnostics reporting.
- Forge may invoke external tools and evaluate generic diagnostics reports through a tool boundary.
- Tools/SagaScript may own Roslyn-backed compile and script artifact manifest emission.
- Saga product orchestration may invoke SagaScript through Forge's generic gate without making Forge own SagaScript behavior.
- Saga package staging may consume script artifact manifests and produce package-specific artifact manifest references.
- Focused Saga package staging tests may compile only the package staging service slice and runtime package manifest loader source without making Forge, SDE, editor, runtime/server, or SagaShared own script compiler behavior.
- Engine may own the runtime/server-facing SagaScript host abstraction, lifecycle orchestration seam, and isolated CSharp runtime host implementation.
- Engine may validate packaged script artifact manifests and referenced script artifact files before passing package load requests to an abstract host.
- Engine may recompute SHA-256 for referenced script assembly artifacts as package-load validation.
- Engine may validate sibling script capability manifests for package-load consistency without adding policy execution.
- Engine may use hostfxr/nethost only inside the isolated `CSharpScriptHost` boundary.
- Focused Saga script lifecycle tests may use a test-only mock host to validate lifecycle and diagnostics behavior without adding editor, Forge, Prism, or Roslyn dependencies.

Forbidden:
- Preserve existing DependencyGraph.md ownership boundaries.
- Keep SDE standalone.
- Keep Forge as an orchestrator only.
- Keep Prism as an analyzer only.
- Keep runtime/server as manifest/artifact consumers.
- Keep editor as authoring UX only.
- Keep SagaShared implementation-free.
- Do not add C# compiler hosts, runtime script host implementation, editor script panels, hot reload implementation, native extension loaders, Roslyn bindings, or CoreCLR bindings to SagaShared.
- Do not make Forge, Prism, Editor, runtime, server, engine, SDE, or SagaShared depend on Roslyn.
- Do not make Forge compile scripts or Prism generate/compile scripts.
- Do not place SagaEngine- or SagaScript-specific implementation files inside Forge; Forge must remain strong enough to work as a separate repository.
- Do not add hot reload, full World/Entity API, full gameplay API, server script execution consumer, editor script panels, Prism analysis, or Forge-owned SagaScript implementation in this slice.

Observed drift:
- Editor currently contains pre-existing `Editor/*/VisualScripting/Runtime/CoreClrHost.*` and `ScriptHost.*` stub files. They are not changed by this slice. They should be reviewed before implementing the scripting toolchain/runtime boundary because the SagaScript roadmap says editor owns authoring UX, not runtime/server script host truth.
- The requested `docs/roadmaps/FORGE_ROADMAP.md` and `docs/roadmaps/PRISM_ROADMAP.md` paths are not present in this tree; the active roadmap files read for this slice were `Tools/Forge/FORGE_ROADMAP.md` and `Tools/Prism/PRISM_ROADMAP.md`.
- `Tools/SagaScript` pins `Microsoft.CodeAnalysis.CSharp` to 4.11.0 by approved user decision. Roslyn references are isolated to `Tools/SagaScript`.
- `Tools/SagaScript` now targets `.NET 10`, and the repo shell now exposes .NET SDK 10.0.202.
- Forge roadmap text still discusses Saga as a consuming project type, but Forge implementation in this slice is generic and does not add SagaScript-specific adapter files or commands.
- Runtime-facing script host ownership is now implemented under `Engine/Public/SagaEngine/Scripting` and `Engine/Private/SagaEngine/Scripting`, matching `ENGINE_ROADMAP.md`'s expected `IScriptHost` seam instead of reusing editor VisualScripting runtime stubs.
```

---

## 7. Diagnostics / Manifests / Reports

Diagnostics:

```txt
- SagaShared SagaScript contracts add `ScriptDiagnosticPayload` as a neutral wrapper around existing diagnostic payloads.
- SagaScript roadmap defines required diagnostic families but adds no diagnostic implementation.
- Tools/SagaScript now emits JSON-compatible diagnostics reports from source validation and manifest emission.
- `sagascript compile` emits compile diagnostics into `sagascript_diagnostics.json` and through the explicit `--diagnostics` path used by the product gate.
- Missing `.NET 10` SDK for `sagascript compile` emits deterministic `Script.Toolchain.DotNetSdkMissing` diagnostics instead of raw SDK target-framework output.
- `ScriptLifecycleService` emits deterministic `Script.Host.InvalidPackageHandle`, `Script.Host.InvalidClassId`, and `Script.Host.InvalidInstanceHandle` diagnostics before dispatching invalid requests to a host.
- `ScriptPackageValidator` emits deterministic `Script.Host.ManifestMissing`, `Script.Host.ManifestInvalid`, `Script.Host.ArtifactMissing`, `Script.Host.InvalidArtifactPath`, `Script.Host.PackageDestinationMismatch`, `Script.Host.UnsupportedTargetFramework`, `Script.Host.InvalidArtifactHash`, `Script.Host.ArtifactHashMismatch`, `Script.Host.InvalidAuthority`, `Script.Host.InvalidCapabilityMetadata`, `Script.Host.CapabilityManifestMissing`, `Script.Host.CapabilityManifestInvalid`, `Script.Host.CapabilityManifestMismatch`, `Script.Host.CapabilityGrantMissing`, and `Script.Host.CapabilityGrantUnexpected` diagnostics before host load.
- `CSharpScriptHost` emits deterministic `Script.Host.RuntimeConfigMissing`, `Script.Host.HostFxrMissing`, `Script.Host.HostFxrInitializationFailed`, `Script.Host.RuntimeDelegateResolutionFailed`, `Script.Host.ManagedBridgeLoadFailed`, `Script.Host.AssemblyLoadFailed`, `Script.Host.ClassMissing`, `Script.Host.ClassInvalid`, `Script.Host.InstanceCreationFailed`, `Script.Host.ManagedException`, and existing invalid handle diagnostics around managed load and lifecycle execution.
- ScriptContext engine callbacks emit deterministic `Script.Host.ScriptCapabilityDenied`, `Script.Host.InvalidEntityHandle`, and `Script.Host.ScriptWorldUnavailable` diagnostics for blocked managed engine operations.
- The test mock host emits deterministic `Script.Host.ClassMissing` and `Script.Host.LifecycleFailed` diagnostics for missing class and lifecycle failure scenarios.
- Forge `gate run` consumes an external diagnostics JSON file by checking a configured boolean key, defaulting to `hasBlockingDiagnostics`.
- CoreCLR-backed diagnostics are currently produced only by the isolated `CSharpScriptHost`; no runtime/server/editor diagnostic producer was added.
```

Manifests:

```txt
- SagaShared SagaScript contracts add in-memory data contracts for script binding manifests, script artifact manifests, and script capability manifests.
- Tools/SagaScript now emits initial JSON-compatible `script_bindings.json` and `script_capabilities.json` manifests.
- `sagascript compile` now emits `script_artifacts.json` and `artifact_manifest.scripts.json` when validated compilation succeeds.
- Saga package staging reads `script_artifacts.json` and writes `artifact_manifest.scripts.client.json` and `artifact_manifest.scripts.server.json` as package-specific manifest refs.
- The script host abstraction consumes package/artifact references through request values only; no script artifact manifest schema redesign was added.
- `ScriptPackageValidator` consumes the existing `script_artifacts.json` and `script_capabilities.json` shapes and validates package-root containment, referenced assembly/runtimeconfig files, `net10.0` target framework, SHA-256 assembly artifact hash metadata and content, authority metadata, capability array shape, capability manifest consistency, exact requested/granted capability set matching, and optional package destination intent before host dispatch.
- `CSharpScriptHost` loads validated script artifacts, invokes managed lifecycle methods through the managed bridge, and exposes the first capability-gated ScriptContext engine API for logging, entity creation, and simple position state; no server script consumer, full gameplay API, hot reload, or sandbox policy was added.
- No Forge SagaScript adapter, Prism analyzer, editor UI, server script execution consumer, or package manifest schema redesign was added.
- Forge generic gate does not know manifest names or package staging paths; Saga product/package orchestration owns the SagaScript-specific wiring.
```

Reports:

```txt
- SagaSync reads export state JSON and displays it as dashboard status only.
- SagaSync previews commit grouping in-memory only and does not write a report schema.
- SagaSync does not add new report schemas.
- SagaScript roadmap defines expected report consumers but adds no report schema.
- SagaShared SagaScript contracts do not add report schemas.
- Tools/SagaScript emits `sagascript_diagnostics.json` as a validation report, not a package or runtime report.
- `sagascript compile` writes script compile diagnostics and package staging writes script artifact manifest refs into `package_stage_report.json`.
- Forge generic gate evaluates an external diagnostics report only as a build gate and does not normalize full tool diagnostics yet.
```

---

## 8. Verification

```txt
Command:
python3 -m py_compile Tools/SagaSync/sagasync.py Tools/SagaSync/core/*.py Tools/SagaSync/ui/*.py Tools/SagaSync/tests/test_sagasync_core.py Tools/SagaTools/setup.py
python3 Tools/SagaSync/tests/test_sagasync_core.py
python3 Tools/SagaSync/sagasync.py --smoke
Tools/SagaSync/sagasync --smoke
cargo check --manifest-path Tools/SagaTools/Cargo.toml
temporary SagaTools registry generation check for `sagasync`
rg -n "git (commit|push|checkout|switch|branch)|gh |GitHub|auto-fix|--fix" Tools/SagaSync
python3 Tools/SagaSync/sagasync.py
git diff --check
git diff --check for SagaScript roadmap-only documentation update
git diff --check --no-index /dev/null docs/roadmaps/SAGA_SCRIPTING_ROADMAP.md
LC_ALL=C rg -n "[^\\x00-\\x7F]" docs/roadmaps/SAGA_SCRIPTING_ROADMAP.md docs/dev/ITERATION_NOTES.md
LC_ALL=C rg -n "[^\\x00-\\x7F]" Shared/include/SagaShared/Scripting Tests/Unit/Shared/SagaScriptContractsTests.cpp
rg -n "SagaEditor|SagaEngine|SagaServer|Forge|Prism|SDE|CoreCLR|Roslyn|Qt" Shared/include/SagaShared/Scripting Tests/Unit/Shared/SagaScriptContractsTests.cpp
env DOTNET_CLI_HOME=/tmp/sagascript-dotnet-home NUGET_PACKAGES=/tmp/sagascript-nuget DOTNET_CLI_TELEMETRY_OPTOUT=1 dotnet build Tools/SagaScript/SagaScript.csproj
python3 Tools/SagaScript/tests/test_sagascript_cli.py
cmake -S Tools/Forge -B Tools/Forge/build
cmake --build Tools/Forge/build --target forge_tool_gate_tests forge_fake_gate_tool forge -j 1
ctest --test-dir Tools/Forge/build -R forge_tool_gate_tests --output-on-failure
ctest --test-dir Tools/Forge/build --output-on-failure
python3 Tools/Forge/build.py -j 1
absolute-path Tools/Forge/bin/forge gate run --explain --name fixture --tool fake --diagnostics /tmp/forge_gate_check.json -- --mode pass
Tools/Forge/bin/forge nix install --jobs=1
Tools/Forge/bin/forge nix install --profile linux-gcc --jobs=1
Tools/Forge/bin/forge nix configure
Tools/Forge/bin/forge nix build -j 1
Tools/Forge/bin/forge nix build --target SagaUnitTests -j 1
Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo -R UnitTests --output-on-failure
LC_ALL=C rg -n "[^\\x00-\\x7F]" Tools/SagaScript .gitignore Shared/include/SagaShared/Scripting Tests/Unit/Shared/SagaScriptContractsTests.cpp
LC_ALL=C rg -n "[^\\x00-\\x7F]" Tools/Forge/include/Forge/Adapters/ExternalGateAdapter.hpp Tools/Forge/src/Adapters/ExternalGateAdapter.cpp Tools/Forge/tests/FakeGateTool.cpp Tools/Forge/tests/ToolGateTests.cpp
rg -n "SagaScript|SagaEngine|sagascript|script_bindings|script_capabilities|Roslyn|Microsoft.CodeAnalysis" Tools/Forge/include Tools/Forge/src Tools/Forge/tests
rg -n "Roslyn|Microsoft.CodeAnalysis|CoreCLR|hostfxr|nethost|Qt|SagaEditor|SagaEngine|SagaServer|Forge|Prism|SDE" Shared/include/SagaShared/Scripting Tests/Unit/Shared/SagaScriptContractsTests.cpp
dotnet --list-sdks
nix-shell -p dotnet-sdk_10 --run 'dotnet --list-sdks'
nix-shell -p dotnet-sdk_10 --run 'env DOTNET_CLI_HOME=/tmp/sagascript-dotnet-home NUGET_PACKAGES=/tmp/sagascript-nuget DOTNET_CLI_TELEMETRY_OPTOUT=1 dotnet build Tools/SagaScript/SagaScript.csproj --nologo -v:minimal'
nix-shell -p dotnet-sdk_10 --run 'env DOTNET_CLI_HOME=/tmp/sagascript-dotnet-home NUGET_PACKAGES=/tmp/sagascript-nuget DOTNET_CLI_TELEMETRY_OPTOUT=1 python3 Tools/SagaScript/tests/test_sagascript_cli.py'
nix-shell -p dotnet-sdk_10 --run "env DOTNET_CLI_HOME=/tmp/sagascript-dotnet-home NUGET_PACKAGES=/tmp/sagascript-nuget DOTNET_CLI_TELEMETRY_OPTOUT=1 Tools/SagaScript/sagascript compile --source '/tmp/sagascript-minimal-check/Scripts/Inventory.cs' --out '/tmp/sagascript-minimal-check/Build/Manifests' --artifacts-out '/tmp/sagascript-minimal-check/Build/Artifacts/Scripts' --project-root '/tmp/sagascript-minimal-check' --diagnostics '/tmp/sagascript-minimal-check/Build/Reports/sagascript_diagnostics.json' --json"
find /tmp/sagascript-minimal-check/Build -maxdepth 4 -type f
sed -n '1,220p' /tmp/sagascript-minimal-check/Build/Manifests/script_artifacts.json
sed -n '1,180p' /tmp/sagascript-minimal-check/Build/Manifests/artifact_manifest.scripts.json
sed -n '1,120p' /tmp/sagascript-minimal-check/Build/Artifacts/Scripts/SagaProjectScripts.scripts.runtimeconfig.json
sed -n '1,180p' /tmp/sagascript-minimal-check/Build/Reports/sagascript_diagnostics.json
stat -c '%n %s bytes' /tmp/sagascript-minimal-check/Build/Artifacts/Scripts/SagaProjectScripts.scripts.dll /tmp/sagascript-minimal-check/Build/Artifacts/Scripts/SagaProjectScripts.scripts.runtimeconfig.json /tmp/sagascript-minimal-check/Build/Manifests/script_artifacts.json /tmp/sagascript-minimal-check/Build/Manifests/artifact_manifest.scripts.json /tmp/sagascript-minimal-check/Build/Reports/sagascript_diagnostics.json
build/RelWithDebInfo/SagaUnitTests --gtest_filter=SagaPackageStagingTest.*
Tools/Forge/bin/forge nix gate run --name=sagascript-smoke --tool=true --diagnostics=<temporary diagnostics.json> --
Tools/Forge/bin/forge nix configure --preset linux-gcc
cmake --build build/RelWithDebInfo --target help | rg "SagaPackageStagingTests|SagaProductTests|SagaUnitTests"
ctest --test-dir build/RelWithDebInfo -N | rg "SagaPackageStagingTests|UnitTests|SagaProductTests"
Tools/Forge/bin/forge nix build --target SagaPackageStagingTests -j 1
Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo -R SagaPackageStagingTests --output-on-failure
build/RelWithDebInfo/SagaPackageStagingTests --gtest_filter=SagaPackageStagingTest.*
Tools/Forge/bin/forge nix configure --preset linux-gcc
Tools/Forge/bin/forge nix build --target SagaScriptLifecycleTests -j 1
Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo -R SagaScriptLifecycleTests --output-on-failure
build/RelWithDebInfo/SagaScriptLifecycleTests --gtest_filter=ScriptLifecycleServiceTests.*
Tools/Forge/bin/forge nix build --target SagaScriptLifecycleTests -j 1
Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo -R SagaScriptLifecycleTests --output-on-failure
build/RelWithDebInfo/SagaScriptLifecycleTests --gtest_filter=ScriptLifecycleServiceTests.*
Tools/Forge/bin/forge nix build --target SagaScriptLifecycleTests -j 1
Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo -R SagaScriptLifecycleTests --output-on-failure
build/RelWithDebInfo/SagaScriptLifecycleTests --gtest_filter=ScriptLifecycleServiceTests.*
LC_ALL=C rg -n "[^\\x00-\\x7F]" Engine/Public/SagaEngine/Scripting Engine/Private/SagaEngine/Scripting Tests/Unit/Runtime/ScriptLifecycleServiceTests.cpp docs/dev/ITERATION_NOTES.md
rg -n "Roslyn|Microsoft\\.CodeAnalysis|CoreCLR|hostfxr|nethost|SagaEditor|Qt" Engine/Public/SagaEngine/Scripting Engine/Private/SagaEngine/Scripting Runtime/include Runtime/src Server/include Server/src Shared/include --glob '!Tools/SagaScript/**'
env DOTNET_CLI_HOME=/tmp/sagascript-dotnet-home NUGET_PACKAGES=/tmp/sagascript-nuget DOTNET_CLI_TELEMETRY_OPTOUT=1 dotnet build Tools/SagaScript/SagaScript.csproj
env DOTNET_CLI_HOME=/tmp/sagascript-dotnet-home NUGET_PACKAGES=/tmp/sagascript-nuget DOTNET_CLI_TELEMETRY_OPTOUT=1 dotnet build Tools/SagaScript/SagaScript.csproj -p:TargetFramework=net8.0
LC_ALL=C rg -n "[^\\x00-\\x7F]" Tools/SagaScript Apps/Saga/SagaScriptGate.cpp Apps/Saga/SagaScriptGate.h Apps/Saga/SagaPackageStaging.cpp Apps/Saga/SagaSessionModel.h Tests/Unit/Saga/SagaProductTests.cpp docs/dev/ITERATION_NOTES.md
rg -n "Roslyn|Microsoft.CodeAnalysis|CoreCLR|hostfxr|nethost" Apps/Saga Engine Server Editor Shared Tools/Forge Tools/Prism Tests/Unit/Saga Tests/Unit/Shared --glob '!Tools/SagaScript/**'
git diff --check
Tools/Forge/bin/forge nix build --target SagaUnitTests -j 1
Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo -R UnitTests --output-on-failure
Tools/Forge/bin/forge nix build -j 1
Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo --output-on-failure
python3 Tools/SagaScript/tests/test_sagascript_toolchain_check.py
bash -n Tools/SagaScript/sagascript
LC_ALL=C rg -n "[^\\x00-\\x7F]" Tools/SagaScript docs/dev/ITERATION_NOTES.md
rg -n "Roslyn|Microsoft.CodeAnalysis|CoreCLR|hostfxr|nethost" Apps/Saga Engine Server Editor Shared Tools/Forge Tools/Prism Tests/Unit/Saga Tests/Unit/Shared --glob '!Tools/SagaScript/**'

Result:
Passed
SagaScript roadmap documentation diff check passed.
SagaScript new-file whitespace check produced no warnings; the no-index command returns nonzero because the file differs from /dev/null.
SagaScript roadmap ASCII scan returned no matches.
SagaScript shared contract ASCII scan returned no matches.
SagaScript shared boundary scan returned no matches.
SagaScript CLI build passed earlier in the iteration before the `.NET 10` retarget, after an approved NuGet restore for `Microsoft.CodeAnalysis.CSharp` 4.11.0.
SagaScript CLI focused test suite passed earlier in the iteration before the `.NET 10` retarget: 9 tests.
Forge standalone CMake configure passed.
Forge generic gate targets built: `forge_tool_gate_tests`, `forge_fake_gate_tool`, and `forge`.
Focused Forge generic gate CTest passed: 1/1.
Standalone Forge CTest suite passed: 16/16.
Forge bootstrap build with `-j 1` passed and staged the updated binary to `Tools/Forge/bin/forge`.
Staged Forge binary `gate run --explain` smoke passed from outside the repository root.
Forge Nix dependency install completed with `--jobs=1`; Qt 6.6.2 was built from source into the local Conan cache.
Forge Nix dependency install with `--profile linux-gcc --jobs=1` generated the expected `build/RelWithDebInfo/generators/conan_toolchain.cmake`.
Forge Nix configure passed.
Focused `SagaUnitTests` target build passed with `-j 1`; the target was already up to date after the interrupted full build.
Focused `UnitTests` CTest run passed: 1/1 tests passed in 3.10 seconds.
SagaScript toolchain ASCII scan returned no matches.
Forge generic gate ASCII scan returned no matches for new files.
Forge implementation/test boundary scan returned no matches for SagaScript/SagaEngine/Roslyn-specific implementation names.
SagaShared scripting boundary scan returned no matches for Roslyn/CoreCLR/Qt/Editor/Engine/Server/Forge/Prism/SDE references.
`git diff --check` passed.
Earlier base-shell SagaScript `.NET 10` build was blocked by missing SDK support; focused verification now passes inside `nix-shell -p dotnet-sdk_10`.
Focused `SagaUnitTests` target build passed with `-j 1` after this slice.
Focused `UnitTests` CTest passed after this slice: 1/1 tests passed in 3.28 seconds.
An earlier full `Tools/Forge/bin/forge nix build -j 1` completed in this iteration; later broad rebuild/CTest attempts were stopped after machine instability.
Current ASCII scan returned no matches for touched SagaScript, Saga product/package, Saga unit test, and iteration note files.
Current boundary scan returned no matches for Roslyn/Microsoft.CodeAnalysis outside `Tools/SagaScript`; hostfxr/nethost usage remains isolated to `CSharpScriptHost` and focused build/test wiring.
Focused SagaScript missing-SDK toolchain check passed: 1 test.
SagaScript launcher Bash syntax check passed.

Current hardening slice status:
Passed:
- `sagascript compile` missing-SDK path now emits `Script.Toolchain.DotNetSdkMissing` with `hasBlockingDiagnostics=true`.
- Missing-SDK diagnostic JSON is written to the requested `--diagnostics` path and printed with `--json`.
- Missing-SDK output does not expose raw `NETSDK1045`.
- Roslyn remains isolated to `Tools/SagaScript`.
- No hot reload, editor UI, full World/Entity API, full gameplay API, replication scripting, or Prism integration was added; runtime hosting and ScriptContext callbacks are limited to the isolated `CSharpScriptHost`.
- .NET SDK 10.0.202 is available through `nix-shell -p dotnet-sdk_10`.
- SagaScript `net10.0` build passed under the .NET 10 shell with 0 warnings and 0 errors.
- Focused SagaScript CLI suite passed under the .NET 10 shell: 10 tests.
- Minimal valid SagaScript compile passed under the .NET 10 shell with 1 artifact and no diagnostics.
- Minimal compile emitted `SagaProjectScripts.scripts.dll`, `SagaProjectScripts.scripts.pdb`, `SagaProjectScripts.scripts.runtimeconfig.json`, `script_artifacts.json`, `artifact_manifest.scripts.json`, and diagnostics JSON.
- `script_artifacts.json` contains `targetFramework: net10.0`, `packageDestinationIntent: server`, relative assembly/runtimeconfig/symbol paths, binding ids, source files, and zero blocking diagnostics.
- `artifact_manifest.scripts.json` contains one `kind: script` artifact referencing `Build/Artifacts/Scripts/SagaProjectScripts.scripts.dll`.
- Runtimeconfig contains `tfm: net10.0` and `Microsoft.NETCore.App` version `10.0.0`.
- Forge generic gate smoke passed through `forge nix gate run` with a temporary non-blocking diagnostics report.
- `SagaPackageStagingTests` target is available in the normal `linux-gcc` build without `SAGA_WITH_SDE=ON`.
- Focused `SagaPackageStagingTests` build passed with `-j 1`; it built 15 steps and did not fan out into the full 795+ step product/engine graph.
- Focused package staging CTest passed: 1/1.
- Direct GTest filter passed: 3 `SagaPackageStagingTest.*` tests, including server-only script artifact placement into the server package manifest only.
- Engine-owned SagaScript host abstraction was added under `Engine/Public/SagaEngine/Scripting` and `Engine/Private/SagaEngine/Scripting`.
- `ScriptLifecycleService` dispatches `OnCreate`, `OnStart`, `OnUpdate(deltaTime)`, and `OnDestroy` through `ISagaScriptHost` without loading CoreCLR or script assemblies.
- `ScriptPackageValidator` validates `script_artifacts.json` package-root containment, referenced `.scripts.dll`, referenced runtimeconfig, and expected package destination before `ISagaScriptHost::LoadPackage` is called.
- `ScriptPackageValidator` validates `targetFramework: net10.0`, non-empty `sha256:` hash metadata, non-empty authority metadata, capability array shape, and client/server authority conflicts before host load.
- Focused `SagaScriptLifecycleTests` build passed with `-j 1`; it built 11 steps and avoided the broad runtime/server/editor/product test graph.
- Final incremental `SagaScriptLifecycleTests` rebuild passed with `-j 1`; it built 7 steps after the last header/doc update.
- Focused script package validation rebuild passed with `-j 1`; it built 14 steps after adding `ScriptPackageValidator`.
- Focused script metadata policy rebuild passed with `-j 1`; it built 7 steps after adding metadata policy checks.
- Focused script lifecycle CTest passed: 1/1.
- Direct script lifecycle GTest filter passed: 19 `ScriptLifecycleServiceTests.*` tests.
- Script lifecycle tests verified deterministic lifecycle order, `OnUpdate` deltaTime forwarding, missing class diagnostics, valid package load reaching the host, missing manifest rejection, missing assembly rejection, escaping artifact path rejection, package destination mismatch rejection, unsupported target framework rejection, missing and malformed hash rejection, missing authority rejection, client/server authority mismatch rejection, malformed capability metadata rejection, shared destination acceptance for client/server, lifecycle failure diagnostics, invalid instance handle rejection, and no editor/runtime-host dependency tokens in the new production scripting boundary.
- Focused `SagaScriptLifecycleTests` build passed with OpenSSL-backed artifact hash recomputation enabled.
- Focused script lifecycle CTest passed after hash recomputation: 1/1.
- Direct script lifecycle GTest filter passed after hash recomputation: 22 `ScriptLifecycleServiceTests.*` tests.
- Script package validation now accepts a valid `.scripts.dll` SHA-256, rejects changed assembly content with `Script.Host.ArtifactHashMismatch`, accepts uppercase/lowercase hash hex case-insensitively, keeps malformed `sha256:` values on `Script.Host.InvalidArtifactHash`, and keeps missing assembly files on `Script.Host.ArtifactMissing`.
- Focused `SagaScriptLifecycleTests` build passed after adding capability manifest validation.
- Focused script lifecycle CTest passed after capability manifest validation: 1/1.
- Direct script lifecycle GTest filter passed after capability manifest validation: 30 `ScriptLifecycleServiceTests.*` tests.
- Script package validation now accepts matching `script_capabilities.json` and rejects missing manifests, malformed manifests, missing script entries, duplicate entries, authority mismatches, package destination mismatches, requested capability mismatches, and granted capability mismatches before host load.
- `Tools/Forge/bin/forge nix build --target SagaScriptLifecycleTests -j 1` passed after strict capability grant enforcement.
- `Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo -R SagaScriptLifecycleTests --output-on-failure` passed after strict capability grant enforcement: 1/1.
- `build/RelWithDebInfo/SagaScriptLifecycleTests --gtest_filter=ScriptLifecycleServiceTests.*` passed after strict capability grant enforcement: 36 tests.
- Script package validation now accepts empty requested/granted sets, accepts exact requested/granted set matches, rejects missing requested grants with `Script.Host.CapabilityGrantMissing`, rejects extra unrequested grants with `Script.Host.CapabilityGrantUnexpected`, and keeps malformed artifact capability arrays on `Script.Host.InvalidCapabilityMetadata`.
- ASCII scan returned no matches for the new engine scripting files, focused script lifecycle test file, and iteration notes.
- Focused production scripting boundary scan returned no matches for Roslyn, Microsoft.CodeAnalysis, SagaEditor, or Qt; hostfxr/nethost references remain confined to the `CSharpScriptHost` boundary and CMake/test wiring.
- `OpenSSL::Crypto` search still finds only the focused `SagaScriptLifecycleTests` CMake link and this iteration note.
- `git diff --check` passed after strict capability grant enforcement.
- `Tools/Forge/bin/forge nix configure` passed after adding the .NET 10 host pack probe.
- `Tools/Forge/bin/forge nix build --target CSharpScriptHostTests -j 1` passed.
- `build/RelWithDebInfo/CSharpScriptHostTests --gtest_filter=CSharpScriptHostTests.*` passed: 14 tests.
- `Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo -R 'SagaScriptLifecycleTests|CSharpScriptHostTests' --output-on-failure` passed: 2/2 focused CTest entries.
- `Tools/Forge/bin/forge nix run python3 Tools/SagaScript/tests/test_sagascript_cli.py` passed: 10 tests.
- `Tools/Forge/bin/forge nix build --target SagaEngine -j 1` passed with pre-existing unrelated engine warnings.
- `nix-shell --run 'dotnet --list-sdks'` reports .NET SDK 10.0.202 from the repository shell.
- CSharpScriptHost now loads a managed `SagaScript`-derived class, creates an instance, invokes `OnCreate`, `OnStart`, `OnUpdate(float)`, and `OnDestroy`, and verifies exact `0.25` delta forwarding through the engine scripting path.
- CSharpScriptHost tests verify deterministic missing class, invalid non-`SagaScript` class, managed lifecycle exception, missing/invalid runtimeconfig, and missing assembly diagnostics.
- CSharpScriptHost ScriptContext tests verify native-observed `Context.Log.Info`, non-null context across lifecycle methods, `Context.World.TryCreateEntity`, `Context.Self`, simple position get/set, exact delta-time position mutation, invalid entity handle diagnostics, and runtime capability denial for ungranted entity creation.
- `Tools/Forge/bin/forge nix build --target CSharpGameplayProofTests -j 1` passed after adding the editorless gameplay proof target.
- `build/RelWithDebInfo/CSharpGameplayProofTests --gtest_filter=CSharpGameplayProofTests.*` passed: 3 tests.
- `Tools/Forge/bin/forge nix run ctest --test-dir build/RelWithDebInfo -R 'SagaScriptLifecycleTests|CSharpScriptHostTests|CSharpGameplayProofTests' --output-on-failure` passed: 3/3 focused CTest entries.
- Editorless gameplay proof tests compile real C# SagaScript source through `Tools/SagaScript/sagascript compile`, mutate a `WorldStateScriptWorld` self entity to `(3, 4, 2)`, create a granted entity at `(7, 8, 9)`, capture managed `Context.Log` events, and reject missing `Gameplay.World.CreateEntity` grants before host load with `Script.Host.CapabilityGrantMissing`.
- `Tools/Forge/bin/forge nix build --target SagaEngine -j 1` passed after the editorless gameplay proof target was added.
- `Tools/Forge/bin/forge nix run python3 Tools/SagaScript/tests/test_sagascript_cli.py` passed after the editorless gameplay proof target was added: 10 tests.
- Current ASCII scan returned no matches for `Tests/Unit/Runtime/CSharpGameplayProofTests.cpp`, `cmake/modules/SagaTests.cmake`, and `docs/dev/ITERATION_NOTES.md`.
- Current production Roslyn boundary scan returned no matches for `Engine`, `Runtime`, `Server`, `Shared`, `Apps/Saga`, or `cmake/modules` outside `Tools/SagaScript`.
- Current scripting runtime/test Editor/Qt scan returned no matches for `Engine/Public/SagaEngine/Scripting`, `Engine/Private/SagaEngine/Scripting`, or `Tests/Unit/Runtime/CSharpGameplayProofTests.cpp`; broader `SagaTests.cmake` matches are existing comments about unrelated editor and architecture test wiring.
- `git diff --check` passed after the editorless gameplay proof target was added.

Blocked:
- Full Saga product/app-entrypoint package staging coverage remains blocked by the broad Saga product shell link graph. An SDE-enabled `SagaProductTests` configure/list path was explored, but building it expanded into a 795-step graph and was not used as the safe verification path.

Not run:
- App entrypoint package staging test through `SagaApp`. Not run.
- Direct package staging consumption of the generated `/tmp/sagascript-minimal-check/Build/Manifests/script_artifacts.json` through the Saga product binary. Not run; the focused fixture covers an equivalent server-only `script_artifacts.json` manifest shape.
- Broad Forge build, full CTest, integration, replication, and stress test reruns for this hardening slice were intentionally avoided after the machine became unstable. Not run.
- Hot reload, editor UI, full gameplay API, networking/replication scripting, server script execution consumer, asset/physics/input/database APIs, and sandbox/security hardening beyond package validation were not added or tested. Not run.
- Runtimeconfig artifact hash recomputation was not added because the current manifest shape has no runtimeconfig hash field. Not run.

Notes:
Python compile checks passed. SagaSync core tests passed with 13 tests. Smoke mode reported the current monorepo, export tools, export states, export health display, commit plan summary, verification profiles, empty verification results, and queue count. SagaTools cargo check passed. Temporary registry generation includes `sagasync`. Safety search returned no matches for blocked mutation/API command tokens. GUI launch was not completed because PySide6 is not installed; the tool exits with a clear dependency message.
`Tools/Forge/bin/forge nix build -j 1` was intentionally limited to one job. It progressed through the full repository build to the unit test target area but was interrupted by the user before completion. Existing warnings were observed in unrelated engine/editor/server/test files and were not changed in this SagaScript contract slice.
An initial `Tools/Forge/bin/forge nix configure` failed because a default install generated `build/Release/generators/conan_toolchain.cmake` while the repository `linux-gcc` preset expects `build/RelWithDebInfo/generators/conan_toolchain.cmake`. Running install with `--profile linux-gcc --jobs=1` produced the expected generator path.
Initial sandboxed `dotnet build Tools/SagaScript/SagaScript.csproj` failed because the .NET SDK tried to write under the user home and then could not restore NuGet packages without network access. The approved rerun used `/tmp` for `DOTNET_CLI_HOME` and `NUGET_PACKAGES` and completed successfully.
An initial multi-target Forge CMake build reconfigured and rebuilt `forge` but did not see the new test target until the standalone Forge CMake project was configured explicitly with `cmake -S Tools/Forge -B Tools/Forge/build`.
After selecting `.NET 10` as the long-term SagaScript baseline, the repository shell now provisions SDK 10.0.202 through `dotnet-sdk_10`; earlier one-off `nix-shell -p dotnet-sdk_10` verification is superseded by the checked-in shell baseline.
After the missing-SDK hardening, `Tools/SagaScript/sagascript compile --json --diagnostics <file>` emits `Script.Toolchain.DotNetSdkMissing` with `hasBlockingDiagnostics=true` before invoking `dotnet run` when no .NET 10 SDK is available.
The full CTest suite was started after the full `forge nix build -j 1`, but the user stopped it after the machine became unstable again. No further broad test run was attempted.
The current `SagaUnitTests` binary does not include `SagaPackageStagingTest.*` because Saga product tests are no longer part of the monolithic unit-test target. Use `SagaPackageStagingTests` for the focused package staging slice.
An SDE-enabled full product test configure path was explored. Its exact SDE Conan package was built once with `tools.build:jobs=1`, but the resulting full product test target still expanded into a broad engine/product graph and was not used for verification.

Not tested:
- Interactive PySide6 GUI behavior, because PySide6 is not installed in this environment.
- Non-Nix host provisioning for `.NET 10`. Not run.
- Package-staging consumption through `SagaApp` app entrypoint. Not run.
- Server script execution consumer, Forge adapter, Prism adapter, runtime/server manifest readers, editor UX behavior, hot reload, native extension execution, and arbitrary untrusted-code sandboxing. Not run.
- Real SagaScript invocation through `forge gate run`; only a generic Forge gate smoke with a temporary diagnostics report was run. Not run.
- Full CTest suite, integration tests, replication tests, and stress tests after this slice; the full CTest run was stopped due machine instability. Not run.
```

---

## 9. Roadmaps To Update

```txt
[ ] EDITOR_ROADMAP.md
[ ] SHARED_ROADMAP.md
[ ] ENGINE_ROADMAP.md
[ ] SDE_ROADMAP.md
[ ] FORGE_ROADMAP.md
[ ] PRISM_ROADMAP.md
[ ] COLLABORATION_ROADMAP.md
[x] TOOLS_ROADMAP.md
[ ] DependencyGraph.md
[ ] DIAGNOSTICS_ROADMAP.md
[ ] ASSET_PIPELINE_ROADMAP.md
[x] SAGA_SCRIPTING_ROADMAP.md
[ ] BUILD_PUBLISH_PIPELINE_ROADMAP.md
[ ] AssetStreamingImplementation.md
[ ] SCHEMA.md
[ ] Other:
```

Reason:

```txt
SagaSync adds an internal tool orchestration dashboard under the tools roadmap.
SagaScript roadmap adds the missing scripting architecture contract referenced by graph, editor, shared, engine, Forge, Prism, collaboration, authority, and persona roadmaps.
SagaShared is affected by the new SagaScript shared contracts, but SHARED_ROADMAP.md was not rewritten in this slice.
Editor roadmap is affected by the observed pre-existing VisualScripting runtime host stubs, but EDITOR_ROADMAP.md was not rewritten in this slice.
Engine roadmap is affected by the new `ISagaScriptHost`, `ScriptLifecycleService`, package validation seam, SHA-256 script assembly artifact validation, capability manifest consistency validation, strict capability grant enforcement, `ScriptContext`, and `WorldStateScriptWorld`, but ENGINE_ROADMAP.md was not rewritten in this slice.
Tools roadmap is affected by the standalone SagaScript CLI, but TOOLS_ROADMAP.md was not rewritten in this slice.
Forge roadmap is affected by the generic external diagnostics gate. The roadmap file was not rewritten in this slice.
Prism roadmap is affected because Prism may later analyze SagaScript outputs, but no Prism integration was added.
Diagnostics and build/publish roadmap areas are affected by the new validation report, manifest gate foundation, and capability grant blocking diagnostics, but their roadmap files were not rewritten in this slice.
```

---

## 10. Known Problems

```txt
- PySide6 is not installed in the current environment unless provided by the user shell.
- Server script execution, hot reload, editor UX behavior, Prism analysis, full gameplay API, full ECS/component exposure, and sandboxing guarantees remain open.
- Editor currently contains pre-existing VisualScripting runtime host stubs. They should be reconciled with the SagaScript ownership model before runtime/server execution work starts.
- `forge nix install` without an explicit profile generated Release Conan files in this environment; use `forge nix install --profile linux-gcc --jobs=1` before `forge nix configure`.
- SagaScript has validation, inspection, manifest, compile/artifact, managed lifecycle, and minimal ScriptContext engine API foundations now, but it does not run through a server script consumer or provide sandboxing guarantees.
- The repository shell now exposes .NET SDK 10.0.202 through `dotnet-sdk_10`.
- `SagaUnitTests` intentionally excludes Saga product tests. Use `SagaPackageStagingTests` for focused package staging coverage and an explicit SDE-enabled product build only for full Saga shell coverage.
- SagaScript v1 type support is intentionally conservative: unsupported types are rejected until a documented shared contract exists.
- Generated-code origin hash mismatch validation is source-level only and requires the referenced file to exist.
- Forge generic gate does not define a package staging model, tool-specific schema, or diagnostics normalization model.
- SagaScript-to-Forge wiring must live outside Forge implementation as project orchestration or explicit tool invocation, not as Forge-owned SagaScript files.
- The script host abstraction validates script artifact manifest/file/destination state, assembly SHA-256 content hashes, local artifact metadata policy, sibling capability manifest consistency, and exact local requested/granted capability sets before package load; `CSharpScriptHost` can now run lifecycle methods and a minimal capability-gated ScriptContext API, but it still does not validate external authority policy files or expose a full gameplay API.
```

---

## 11. Next Actions

```txt
[x] Implement SagaSync MVP foundation.
[x] Verify SagaSync core, SagaTools registration, and Python syntax.
[x] Add SagaScript roadmap contract.
[x] Add SagaShared SagaScript data-only contracts and focused tests.
[x] Verify focused SagaScript shared contract slice through `SagaUnitTests`.
[x] Add standalone SagaScript CLI foundation for validation, binding inspection, manifest emission, and diagnostics.
[x] Verify focused SagaScript CLI tests and focused Forge `SagaUnitTests`.
[x] Add Forge generic external diagnostics gate without SagaEngine/SagaScript implementation coupling.
[x] Verify standalone Forge generic gate tests and standalone Forge CTest suite.
[x] Add SagaScript compile/artifact manifest foundation behind Tools/SagaScript.
[x] Wire Saga product SagaScript validation to `sagascript compile` through Forge's generic gate.
[x] Add package staging split for client/server script artifact manifests.
[x] Verify SagaScript compile tests after a .NET 10 SDK is available in the repository Nix shell.
[x] Verify focused Saga package staging GTests without triggering a broad machine-heavy rebuild.
[x] Add editor-independent SagaScript host abstraction and mock lifecycle tests without CoreCLR execution.
[x] Verify focused SagaScript lifecycle tests without triggering a broad machine-heavy rebuild.
[x] Add script artifact/package validation before lifecycle package load without CoreCLR execution.
[x] Verify focused script package validation tests without triggering a broad machine-heavy rebuild.
[x] Add script artifact metadata policy validation before lifecycle package load without CoreCLR execution.
[x] Verify focused script metadata policy tests without triggering a broad machine-heavy rebuild.
[x] Add OpenSSL EVP SHA-256 recomputation for script assembly artifacts before lifecycle package load.
[x] Verify focused script artifact hash mismatch, malformed hash, missing file, and case-insensitive hash tests.
[x] Add sibling script capability manifest consistency validation before lifecycle package load.
[x] Verify missing, malformed, duplicate, and mismatched capability manifest tests.
[x] Add managed `SagaScript` base class contract and runtime bridge.
[x] Add hostfxr/nethost-backed `CSharpScriptHost` behind `ISagaScriptHost`.
[x] Verify focused CSharp script host lifecycle execution and diagnostics.
[x] Add managed `ScriptContext` with log, self, world entity create, and simple position API.
[x] Add capability-gated native callbacks and `WorldStateScriptWorld` adapter for ScriptContext V1.
[x] Verify focused ScriptContext CSharp host engine API tests and existing lifecycle validator tests.
[x] Add editorless C# gameplay proof target that compiles real SagaScript source and runs through `ScriptLifecycleService`, `CSharpScriptHost`, and `WorldStateScriptWorld`.
[x] Verify editorless C# gameplay proof target, existing focused lifecycle/CSharp host CTest entries, SagaScript CLI tests, engine build, boundary scans, ASCII scan, and diff whitespace.
[ ] Decide whether full Saga app-entrypoint package staging coverage needs a smaller product-shell test seam.
[ ] Review and reconcile pre-existing Editor VisualScripting runtime host stubs against the SagaScript ownership model.
[ ] Decide the next bounded integration point: server script execution consumer, package-entrypoint seam, external authority policy source, Prism manifest reader, or Editor stub reconciliation.
```

---

## 12. Roadmap Update Request

Use this block when updating roadmaps from this iteration.

```txt
Read docs/dev/ITERATION_NOTES.md.

Update only the roadmap files marked in section 9.

Rules:
- Do not rewrite unrelated sections.
- Do not mark unverified work as shipped.
- If status is Draft or Partially Implemented, keep roadmap items open.
- Preserve DependencyGraph.md ownership boundaries.
- Keep SDE standalone.
- Keep Forge as orchestrator only.
- Keep Prism as analyzer only.
- Keep runtime/server as manifest/artifact consumers.
- Keep editor as authoring UX only.
- Keep SagaShared implementation-free.
```

---

## 13. Next Iteration

```txt
Next iteration: 0.0.8-dev.8

Possible focus:
- Add a smaller product-shell/app-entrypoint test seam if package staging must be covered through `SagaApp` without building the full Saga product graph.
- Keep Forge generic gate as the boundary; continue wiring SagaScript from Saga/project orchestration rather than adding Saga-specific Forge files.
- Add broader gameplay API surface only after the minimal ScriptContext bridge remains stable.
- Add runtime manifest validation or an external authority policy source before broader runtime/server use.
- Add Prism script/package manifest analysis or runtime manifest validation now that focused package staging and lifecycle/CSharp host GTests are green.
- Reconcile pre-existing Editor VisualScripting runtime host stubs before any runtime/server execution work.
```
