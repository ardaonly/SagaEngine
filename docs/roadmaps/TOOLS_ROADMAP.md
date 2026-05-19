# Saga Tools Roadmap

> Last updated: 2026-05-15
> Status: Active roadmap
> Target: A coherent tool ecosystem for Saga that keeps standalone tools reusable, ownership boundaries explicit, and product/editor/runtime/server responsibilities separated.
> Scope: SDE, Forge, Prism, SagaTools, AssetPipeline tools, Scripting toolchain, Host tooling, export/mirroring tooling, packaging/report tooling, CI commands, tool environment discovery, command dispatch, diagnostics, integration contracts, and dependency boundaries.

---

## 0. Roadmap Convention

* `[x]` — Shipped. The note after the item names files, commands, tests, packages, or integration points that represent completed work.
* `[ ]` — Open. The item describes finished production behavior, not temporary scaffolding.
* Tool work must name owner, command surface, input/output contracts, diagnostics, tests, and integration boundaries where practical.
* Tools must remain independently understandable.
* Tools must not quietly become product shell, editor UI, runtime/server implementation, or another tool's internals.
* A tool may invoke another tool through command/service boundaries where approved.
* A tool must not include another tool's private implementation just because linking was convenient.

A tool ecosystem fails when every tool can technically do everything.

That is not flexibility.

That is a drawer full of knives with no handles.

---

## 1. Document Purpose

This document defines the roadmap for the Saga tool ecosystem.

The tool ecosystem exists to support project creation, data compilation, build orchestration, code/artifact intelligence, asset import/cook workflows, script compilation, package staging, publishing checks, host/server development workflows, and repository/tool mirroring.

This document is an ownership index.

It answers:

```txt
Which tool owns which job?
Which tool may invoke which other tool?
Which outputs are consumed by product/editor/runtime/server?
Which contracts are shared?
Which dependencies are forbidden?
```

Correct model:

```txt
SDE
  standalone deterministic compiler

Forge
  build workflow frontend/orchestrator

Prism
  code/artifact intelligence analyzer

SagaTools
  thin dispatcher / installed command surface

AssetPipeline
  import/cook/artifact generation tools

Scripting Toolchain
  script validation/compile/binding generation tools

Host Tooling
  local server/service orchestration for development
```

Incorrect model:

```txt
Forge compiles SDE internals.
Prism builds packages.
SDE knows SagaEngine runtime headers.
SagaTools becomes product shell.
AssetPipeline becomes runtime loader.
Script compiler becomes editor UI.
```

That is not integration.

That is ownership collapse.

---

## 2. Companion Documents

| Document                            | Purpose                                                        |
| ----------------------------------- | -------------------------------------------------------------- |
| `SDE_ROADMAP.md`                    | Standalone deterministic compiler                              |
| `FORGE_ROADMAP.md`                  | Build workflow frontend and package/publish orchestration      |
| `PRISM_ROADMAP.md`                  | Code/artifact intelligence and stale/boundary analysis         |
| `SAGATOOLS_ROADMAP.md`              | Thin installed dispatcher / command registry                   |
| `ASSET_PIPELINE_ROADMAP.md`         | Source asset import/cook/runtime-ready artifact pipeline       |
| `SAGA_SCRIPTING_ROADMAP.md`         | C# scripting, script compiler, bindings, hot reload boundaries |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md` | Full validation/build/cook/analyze/package/publish pipeline    |
| `SAGA_PRODUCT_ROADMAP.md`           | Product shell that initiates and displays workflows            |
| `EDITOR_ROADMAP.md`                 | Editor authoring UX consuming tool diagnostics/reports         |
| `SHARED_ROADMAP.md`                 | Shared contracts used by tool reports/manifests                |
| `ENGINE_ROADMAP.md`                 | Runtime/server consuming packaged artifacts/manifests          |
| `COLLABORATION_ROADMAP.md`          | Collaboration state consumed by build/publish gates            |
| `DependencyGraph.md`                | Dependency ownership and forbidden edges                       |

---

## 3. Tool Ownership Summary

| Tool / Area             | Primary Job                                                     | Must Not Become                                                           |
| ----------------------- | --------------------------------------------------------------- | ------------------------------------------------------------------------- |
| `SDE`                   | Standalone deterministic definition compiler                    | SagaEngine subsystem, editor graph backend, runtime VM                    |
| `Forge`                 | Build workflow frontend, orchestrator, package/publish reports  | CMake replacement, SDE compiler, Prism indexer, asset cooker, C# compiler |
| `Prism`                 | Source/artifact intelligence, stale checks, boundary validation | Build system, compiler, package stager, asset cooker                      |
| `SagaTools`             | Thin dispatcher and installed command surface                   | Product shell, build planner, compiler, analysis engine                   |
| `AssetPipeline`         | Source asset import/cook to runtime-ready artifacts             | Runtime loader, editor content browser, Forge internals                   |
| `Scripting Toolchain`   | Script validate/compile/analyze/binding manifest generation     | Runtime script host, editor script UI, Forge internals                    |
| `Host Tooling`          | Local dev server/service orchestration                          | Production deployment platform, runtime/server implementation             |
| `Export/Mirror Tooling` | Push tool mirrors to external repos                             | Build system, tool implementation owner                                   |

---

## 4. Global Tool Dependency Rules

### 4.1 Allowed Patterns

Allowed:

```txt
Saga → invokes tools through service/command boundaries
SagaEditor → invokes tools through Saga/Forge/service boundaries and consumes reports
Forge → invokes SDE executable/public facade
Forge → invokes Prism executable/report boundary
Forge → invokes AssetPipeline executable/service boundary
Forge → invokes Scripting compiler executable/service boundary
Forge → invokes CMake/Conan through adapters
Prism → reads manifests/reports/source files
Runtime/Server → consume packaged artifacts/manifests, not tool internals
SagaTools → dispatches installed tools
```

---

### 4.2 Forbidden Patterns

Forbidden:

```txt
SDE → Saga/SagaEngine/SagaEditor/SagaServer/SagaShared/Forge/Prism/SagaTools
Forge → SDE parser/AST/semantic internals
Prism → SDE parser/AST/semantic internals
Prism → Forge planner internals
AssetPipeline → Runtime private asset streaming cache
ScriptingCompiler → Editor script panel implementation
SagaTools → internal implementation of SDE/Forge/Prism/AssetPipeline/Scripting
Runtime/Server → Forge/Prism/SDE internals
Editor public headers → tool implementation internals
```

Forbidden shortcuts:

```txt
A tool mutates outputs owned by another tool without explicit command boundary.
A tool fixes stale artifacts by silently regenerating them unless that is its job.
A tool includes private headers from another tool to avoid writing a manifest reader.
A product/editor workflow treats terminal output as the only machine-readable report.
```

---

## 5. SDE Tool Ownership

* [x] Keep SDE as standalone deterministic compiler.

  Represented by:

  ```txt
  Tools/SystemDefinitionEngine/
  SDE::Core
  conanfile.py
  build.py
  SDEConfig.cmake
  version.json
  CHANGELOG.md
  ```

  Preserved decision:

  ```txt
  SDE can stand alone and must not be locked to SagaEngine.
  ```

* [ ] Provide stable SDE CLI.

  Required commands:

```txt
sde help
sde version
sde validate
sde compile
sde inspect
sde format
sde doctor
```

* [ ] Provide stable outputs.

  Required outputs:

```txt
canonical IR
graph artifacts
artifact manifests
source maps
dependency manifests
diagnostics JSON
```

* [ ] Keep Saga-specific behavior as schema packages/adapters.

  Done means Saga-specific concepts are not hardcoded into SDE core implementation.

Expected owner docs:

```txt
SDE_ROADMAP.md
```

Expected paths:

```txt
Tools/SystemDefinitionEngine/include/SDE/
Tools/SystemDefinitionEngine/src/
Tools/SystemDefinitionEngine/docs/
Tools/SystemDefinitionEngine/examples/
Tools/SystemDefinitionEngine/tests/
```

---

## 6. Forge Tool Ownership

* [x] Keep Forge as build workflow frontend.

  Represented by:

  ```txt
  Tools/Forge/
  BuildModel
  CMakeAdapter
  ConanAdapter
  ToolEnv
  EnvProbe
  forge.lock
  ```

  Preserved decision:

  ```txt
  Forge coordinates build workflows through adapters and reports.
  ```

* [ ] Add full build plan/pipeline model.

  Done means Forge has:

```txt
BuildModel
BuildPlan
BuildStep
BuildReport
PublishReport
ArtifactManifest
PackageManifest
```

* [ ] Add adapters for Saga pipeline tools.

  Required adapters:

```txt
SdeAdapter
PrismAdapter
AssetPipelineAdapter
ScriptCompilerAdapter
PackageStager
```

* [ ] Add package/publish commands.

  Required commands:

```txt
forge validate
forge package
forge publish-check
forge report
forge doctor
```

* [ ] Keep Forge useful for standalone C++ projects.

  Done means Saga-specific features are opt-in and ordinary CMake/Conan workflow remains usable.

Expected owner docs:

```txt
FORGE_ROADMAP.md
BUILD_PUBLISH_PIPELINE_ROADMAP.md
```

Expected paths:

```txt
Tools/Forge/include/Forge/
Tools/Forge/src/
Tools/Forge/tests/
```

---

## 7. Prism Tool Ownership

* [x] Keep Prism as code/artifact intelligence tool.

  Represented by:

  ```txt
  Tools/Prism/
  PRISM_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  Prism observes, indexes, validates relationships, and reports.
  Prism does not compile/build/cook/package.
  ```

* [ ] Add stable Prism CLI.

  Required commands:

```txt
prism help
prism version
prism index
prism symbols
prism deps
prism boundaries
prism stale
prism generated-origin
prism artifacts
prism packages
prism report
prism doctor
```

* [ ] Add analysis families.

  Required analysis:

```txt
C++ include graph
C# source/block-callable index
SDE manifest/source map analysis
graph/generated code origin tracking
script binding manifest consistency
asset/cooked artifact stale checks
package manifest consistency
architecture boundary validation
```

* [ ] Emit machine-readable reports.

  Required reports:

```txt
index_report.json
dependency_report.json
boundary_report.json
stale_report.json
generated_origin_report.json
artifact_relationship_report.json
package_relationship_report.json
prism_diagnostics.json
```

Expected owner docs:

```txt
PRISM_ROADMAP.md
```

Expected paths:

```txt
Tools/Prism/include/Prism/
Tools/Prism/src/
Tools/Prism/tests/
```

---

## 8. SagaTools Ownership

* [x] Keep SagaTools as thin dispatcher / command registry.

  Represented by:

  ```txt
  Tools/SagaTools/
  Tools/SagaTools/setup.py
  Tools/SagaTools/bin/sagatools
  sagatools list
  sagatools <tool> help
  ```

  Preserved decision:

  ```txt
  SagaTools dispatches tools. It does not implement them.
  ```

* [ ] Register all major tools through SagaTools.

  Target tools:

```txt
sde
forge
prism
sagasync
asset-pipeline
script-compiler
host
export
```

* [ ] Keep SagaTools metadata declarative.

  Done means SagaTools records:

  * tool name,
  * executable path,
  * help command,
  * version command,
  * category,
  * description,
  * install/check status.

* [ ] Avoid SagaTools becoming product shell.

  Done means SagaTools does not own:

  * project dashboard,
  * editor UI,
  * build planning,
  * compile logic,
  * analysis logic,
  * package staging,
  * runtime/server lifecycle.

Expected owner docs:

```txt
SAGATOOLS_ROADMAP.md
```

Expected paths:

```txt
Tools/SagaTools/
Tools/SagaTools/setup.py
Tools/SagaTools/bin/
```

---

## 9. AssetPipeline Tool Ownership

* [ ] Define AssetPipeline as import/cook/artifact-generation tooling.

  Done means AssetPipeline owns:

  * source asset discovery,
  * importer registry,
  * import settings validation,
  * asset metadata generation,
  * cook planning,
  * cook execution,
  * cooked artifact emission,
  * asset manifest generation,
  * asset diagnostics,
  * asset budget data.

* [ ] Keep AssetPipeline separate from runtime asset streaming.

  Done means:

  * AssetPipeline converts source assets to runtime-ready artifacts,
  * runtime streaming consumes cooked artifacts and manifests,
  * runtime does not own source import/cook behavior.

* [ ] Keep AssetPipeline separate from editor UX.

  Done means:

  * editor owns content browser/import UI,
  * AssetPipeline owns tool/service behavior,
  * editor invokes AssetPipeline through service/tool boundary.

* [ ] Add AssetPipeline CLI or service interface.

  Candidate commands:

```txt
asset-pipeline validate
asset-pipeline import
asset-pipeline cook
asset-pipeline inspect
asset-pipeline doctor
```

Expected owner docs:

```txt
ASSET_PIPELINE_ROADMAP.md
```

Expected paths:

```txt
Tools/AssetPipeline/include/SagaAssetPipeline/
Tools/AssetPipeline/src/
Tools/AssetPipeline/tests/
```

---

## 10. Scripting Toolchain Ownership

* [ ] Define scripting toolchain as script validation/compile/binding artifact owner.

  Done means scripting toolchain owns:

  * script project discovery,
  * script manifest validation,
  * C# compile invocation/tooling,
  * script analyzers,
  * block-callable extraction,
  * script assembly artifact emission,
  * binding manifest generation,
  * authority metadata extraction/validation,
  * generated code manifest emission,
  * script diagnostics.

* [ ] Keep scripting compiler separate from runtime script host.

  Done means:

  * scripting toolchain compiles/analyzes scripts,
  * runtime/server script host loads/binds validated artifacts,
  * compiler and runtime host do not collapse into one ownership blob.

* [ ] Keep scripting toolchain separate from editor script UI.

  Done means:

  * editor owns script editing panels,
  * scripting toolchain owns compile/analyze behavior,
  * editor consumes diagnostics and binding manifests.

* [ ] Add script compiler CLI or service interface.

  Candidate commands:

```txt
script-compiler validate
script-compiler compile
script-compiler inspect-bindings
script-compiler doctor
```

Expected owner docs:

```txt
SAGA_SCRIPTING_ROADMAP.md
```

Candidate paths:

```txt
Tools/Scripting/
Tools/Scripting/include/SagaScriptingToolchain/
Tools/Scripting/src/
Tools/Scripting/tests/
```

or, if initially embedded:

```txt
Engine/include/SagaEngine/Scripting/   runtime host only
Tools/Scripting/                       compiler/toolchain side
```

---

## 11. Host Tooling Ownership

* [x] Add initial Host tooling for local infrastructure control.

  Represented by:

  ```txt
  Tools/Host/host.sh
  Tools/Host/docker/docker-compose.yml
  Tools/Host/docker/Dockerfile
  Tools/Host/README.md
  Tools/SagaTools setup registration
  ```

  Preserved decision:

  ```txt
  Host tooling manages local development services and should integrate with SagaTools.
  ```

* [ ] Harden Host tooling.

  Done means host tooling supports:

```txt
start
stop
restart
rebuild
logs
status
health-check
config validation
clear diagnostics
```

* [ ] Keep Host tooling development-focused unless explicitly promoted.

  Done means Host tooling does not become production deployment platform without a separate deployment roadmap.

* [ ] Add structured reports where useful.

  Done means host status can be consumed by Saga product dashboard or diagnostics if needed.

Expected paths:

```txt
Tools/Host/
Tools/Host/host.sh
Tools/Host/docker/
Tools/Host/README.md
```

---

## 12. Export and Mirror Tooling

* [x] Maintain tool mirror/export script path.

  Represented by:

  ```txt
  export.sh
  Tools/scripts/export_tool_mirrors.py
  ./export.sh --dry-run --tool Prism
  ```

  Preserved decision:

  ```txt
  Tool repos can be mirrored/exported independently.
  ```

* [ ] Harden export workflow.

  Done means export tooling supports:

  * dry-run,
  * per-tool export,
  * manifest validation,
  * git cleanliness checks,
  * explicit destination repo config,
  * no accidental push by default,
  * structured summary.

  `0.0.8-dev.7` added SagaSync as a PySide6 internal dashboard foundation for
  this workflow. It reads export manifest/state data, shows conservative
  commit/export queue suggestions, evaluates export health, and runs named
  dry-run/read-only verification profiles with session-local run history and
  unverified/stale health display. It also previews read-only commit groups
  with suggested messages. It does not stage, commit, or push.

* [ ] Ensure new tools are export-aware where needed.

  Candidate exported tools:

```txt
SDE
Forge
Prism
SagaTools
AssetPipeline
Scripting Toolchain
Host
```

* [ ] Keep export tooling separate from build tooling.

  Done means export copies/pushes tool repositories; it does not compile or validate tool internals beyond explicit checks.

---

## 13. Report and Manifest Tooling

* [ ] Standardize tool report outputs.

  Done means major tools can emit:

```txt
--json
--report <path>
--explain
stable exit codes
machine-readable diagnostics
```

* [ ] Standardize report families.

  Required report families:

```txt
sde_diagnostics.json
sde_artifact_manifest.json
sde_dependency_manifest.json
build_report.json
diagnostics_report.json
publish_report.json
prism_stale_report.json
prism_boundary_report.json
asset_manifest.json
script_bindings.json
package_manifest.client.json
package_manifest.server.json
```

* [ ] Keep report schemas versioned.

  Done means reports include:

  * tool name,
  * tool version,
  * schema/report version,
  * project root,
  * profile,
  * generated time where useful,
  * diagnostics,
  * inputs,
  * outputs.

---

## 14. Tool Environment and Discovery

* [x] Use Forge `ToolEnv` for build tool executable resolution.

* [ ] Add global tool discovery model.

  Done means Saga/SagaTools/Forge can discover:

  * installed SDE,
  * installed Forge,
  * installed Prism,
  * asset pipeline tool,
  * script compiler tool,
  * host tool,
  * configured tool overrides.

* [ ] Add version compatibility checks.

  Done means tools can report:

  * tool version,
  * supported manifest version,
  * supported artifact version,
  * compatibility status.

* [ ] Add `doctor` behavior across tools.

  Done means each major tool can check its own health and dependencies.

---

## 15. CI Tooling

* [ ] Define canonical CI commands.

  Required commands:

```txt
sde validate
forge validate --profile ci
forge build --profile test
forge publish-check --profile shipping-full
prism boundaries --profile ci
prism stale --profile ci
asset-pipeline validate
script-compiler validate
```

* [ ] Define stable exit code families.

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
InternalError
UserCancelled
```

* [ ] Emit CI artifacts.

  Required CI artifacts:

```txt
build_report.json
diagnostics_report.json
publish_report.json
boundary_report.json
stale_report.json
sde_diagnostics.json
asset_diagnostics.json
script_diagnostics.json
```

---

## 16. Product and Editor Integration

* [ ] Keep Saga product shell as tool workflow initiator/presenter.

  Done means Saga can start workflows and display status but does not implement tool internals.

* [ ] Keep SagaEditor as diagnostics/report consumer.

  Done means editor can show tool reports in:

  * Problems panel,
  * Build/Publish panel,
  * Diagnostics Report panel,
  * Graph editor,
  * Script editor,
  * Asset inspector.

* [ ] Tools must emit editor-consumable locations.

  Done means diagnostics can reference:

  * source file/range,
  * graph node/pin/edge,
  * script source/range,
  * asset id/path,
  * package manifest entry,
  * build step,
  * artifact id/path.

---

## 17. Runtime and Server Integration

* [ ] Keep runtime/server as artifact consumers.

  Done means runtime/server consume:

  * package manifests,
  * asset manifests,
  * graph artifacts,
  * script binding manifests,
  * authority manifests,
  * diagnostics metadata where useful.

* [ ] Runtime/server must not depend on tool internals.

  Forbidden:

```txt
Runtime → SDE internals
Runtime → Forge internals
Runtime → Prism internals
Runtime → AssetPipeline implementation
Runtime → Scripting compiler implementation
Server → same forbidden internals
```

* [ ] Provide startup validation independent of tools.

  Done means runtime/server validate packaged artifacts/manifests at startup without needing Forge or Prism internals.

---

## 18. Testing Strategy

### 18.1 Tool Boundary Tests

* [ ] Add cross-tool dependency boundary tests.

  Required checks:

  * SDE does not include Saga modules,
  * Forge does not include SDE internals,
  * Prism does not include SDE/Forge internals,
  * AssetPipeline does not include runtime private cache internals,
  * Scripting toolchain does not include editor script UI,
  * SagaTools does not include tool internals,
  * Runtime/server do not include tool internals.

---

### 18.2 Command Smoke Tests

* [ ] Add command smoke tests.

  Required commands:

```txt
sde version
sde validate --help
forge version
forge validate --help
prism version
prism stale --help
sagatools list
asset-pipeline --help
script-compiler --help
```

---

### 18.3 Report Schema Tests

* [ ] Add report schema tests.

  Required coverage:

  * SDE diagnostics report,
  * Forge build report,
  * Forge publish report,
  * Prism stale report,
  * Prism boundary report,
  * asset manifest,
  * script binding manifest,
  * package manifest.

---

### 18.4 Integration Tests

* [ ] Add minimal Saga toolchain integration test.

  Required flow:

```txt
sde compile fixture
forge build fixture
prism stale fixture
asset-pipeline cook fixture
script-compiler compile fixture
forge publish-check fixture
```

* [ ] Add standalone external tool tests.

  Required coverage:

  * SDE external project sample,
  * Forge standalone C++ sample,
  * Prism standalone C++ boundary sample.

---

## 19. MVP Toolchain Vertical Slice

The first full toolchain slice should process a minimal Saga project.

Required project:

```txt
saga.project.json
forge.toml
.sde/quest_reward.sde
Scripts/QuestXp.cs
Assets/terrain_albedo.png
```

Required commands:

```txt
sde validate .sde
sde compile .sde --out Build/Artifacts/SDE
script-compiler compile Scripts --out Build/Artifacts/Scripts
asset-pipeline cook Assets --out Build/Artifacts/Assets
prism stale --profile dev-local-client-server
forge build --profile dev-local-client-server
forge publish-check --profile shipping-full
sagatools list
```

Required behavior:

1. SDE compiles definitions and graph artifacts.
2. Script toolchain emits script assembly/binding manifests.
3. AssetPipeline emits cooked texture artifact and asset manifest.
4. Prism validates stale/generated/package relationships.
5. Forge orchestrates build/package/publish-check.
6. SagaTools lists and dispatches major tools.
7. Reports are machine-readable.
8. Runtime/server can consume package manifests without depending on tools.

This slice proves the tool ecosystem has ownership boundaries and real integration.

Not just a pile of impressive executables.

---

## 20. Non-Goals

The tool ecosystem must not become:

* one mega-tool that owns everything,
* a set of tools that all duplicate each other's internals,
* a replacement for Saga product shell,
* a replacement for SagaEditor UI,
* runtime/server implementation,
* a build pipeline based only on shell-script folklore,
* a collection of terminal outputs with no structured reports,
* a hidden dependency trap for standalone tools.

Each tool should do its job well and expose stable contracts.

The coordination layer should be explicit.

---

## 21. Risk Register

### 21.1 Risk: Tools Collapse Into Each Other

Mitigation:

* explicit owner docs,
* adapters instead of private includes,
* boundary tests,
* report/manifest contracts.

---

### 21.2 Risk: SDE Becomes Saga-Locked

Mitigation:

* standalone package tests,
* non-Saga examples,
* no Saga module dependencies,
* schema package model.

---

### 21.3 Risk: Forge Becomes Everything

Mitigation:

* Forge invokes tools through adapters,
* SDE/Prism/AssetPipeline/Scripting stay separate,
* Forge owns orchestration/reports/gates only.

---

### 21.4 Risk: Prism Becomes Build System

Mitigation:

* Prism reports only,
* no artifact generation,
* no package staging,
* Forge decides gate behavior.

---

### 21.5 Risk: Runtime Depends on Tool Internals

Mitigation:

* runtime consumes package/artifact manifests,
* startup validation is independent,
* forbid tool internals in runtime/server.

---

### 21.6 Risk: Reports Are Not Machine-Readable

Mitigation:

* require JSON/report outputs,
* schema versioning,
* CI artifacts,
* editor navigation payloads.

---

## 22. Suggested Repository Layout

Expected existing/target layout:

```txt
Tools/
  Forge/
  Prism/
  SagaTools/
  SystemDefinitionEngine/
  AssetPipeline/
  Scripting/
  Host/
  scripts/
```

Expected high-level commands:

```txt
sde
forge
prism
sagatools
asset-pipeline
script-compiler
host
```

Expected shared report roots in projects:

```txt
<Project>/Build/Reports/
<Project>/Build/Manifests/
<Project>/Build/Artifacts/
<Project>/Packages/
```

---

## 23. Decision Summary

Preserve these decisions:

```txt
SDE is standalone deterministic compiler.
Forge is build workflow frontend/orchestrator.
Prism is code/artifact intelligence analyzer.
SagaTools is thin dispatcher.
AssetPipeline owns import/cook/artifact generation.
Scripting toolchain owns script compile/binding generation.
Host tooling owns local development service orchestration.
Export tooling owns tool mirror/export workflows.
Tools communicate through CLIs, adapters, manifests, reports, and shared contracts.
Tools do not include each other's private internals.
Saga product shell initiates/displays workflows.
SagaEditor consumes diagnostics/reports.
Runtime/server consume packaged artifacts/manifests.
```

The tool ecosystem should feel coordinated without becoming monolithic.

The serious version of this architecture is not “one tool to rule them all.”

It is many tools with clean contracts and no ownership cosplay.
