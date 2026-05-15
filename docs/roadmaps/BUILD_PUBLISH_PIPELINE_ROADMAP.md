# Saga Build and Publish Pipeline Roadmap

> Last updated: 2026-05-15
> Status: Proposed production roadmap
> Target: A deterministic, diagnosable, CI-friendly build and publish pipeline that validates, compiles, cooks, analyzes, stages, packages, and publish-checks Saga projects across gameplay graphs, SDE data, C# scripts, assets, runtime/server artifacts, collaboration state, and diagnostics.
> Scope: Project validation, SDE validation/compile, gameplay graph artifacts, authority validation, script compile, asset cook, Prism stale/boundary analysis, runtime/server package staging, manifest generation, diagnostics aggregation, CI profiles, preview builds, publish readiness checks, and product-facing failure UX.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names concrete files, modules, commands, tests, artifacts, reports, or integration points that represent completed work.
- `[ ]` — Open. The item describes the finished production state, not temporary scaffolding.
- Shipped build/publish work must include implementation, diagnostics, tests, and reproducible command evidence.
- Open build/publish work must describe observable behavior, not vague workflow intent.
- Forge owns build orchestration.
- Saga owns product-facing build/publish entry points and user-facing publish readiness.
- SDE owns deterministic data/graph compilation.
- Prism owns analysis, stale output detection, and boundary validation.
- Runtime/server consume packaged artifacts.
- The editor may trigger build/publish workflows, but it does not own the product build pipeline.

A build pipeline that only works when the developer remembers the correct ritual is not a build pipeline.

It is folklore with shell scripts.

---

## 1. Document Purpose

This document defines Saga's build and publish pipeline roadmap.

The build/publish pipeline connects the major Saga systems into one product-validating chain:

```txt
Project Manifest
    ↓
Workspace Resolution
    ↓
Project Validation
    ↓
SDE Validate / Compile
    ↓
Gameplay Graph + Authority Validation
    ↓
C# Script Validate / Compile
    ↓
Asset Validate / Cook
    ↓
Prism Boundary / Stale Artifact Checks
    ↓
Package Staging
    ↓
Runtime/Server Manifest Generation
    ↓
Publish Readiness Check
    ↓
Publish / Export / Deployment Handoff
```

The target is a pipeline where a project can be checked and packaged reproducibly, with clear diagnostics and no silent stale artifact state.

Correct model:

```txt
Saga initiates and displays build/publish workflow.
Forge creates and runs build plans.
SDE compiles deterministic data/graph artifacts.
Script compiler produces script artifacts and binding manifests.
Asset pipeline produces cooked runtime-ready assets.
Prism verifies stale outputs and boundary relationships.
Runtime/server packages consume explicit manifests.
Publish is blocked by invalid graph/script/asset/collaboration/manifest state.
```

Incorrect model:

```txt
Click Play, hope old generated files are still correct, and ship whatever is in Build/.
```

That is not publishing.

That is archeology with consequences.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `SAGA_PRODUCT_ROADMAP.md` | Product shell, project lifecycle, build/publish entry points |
| `FORGE_ROADMAP.md` | Build workflow frontend, build plan, validation, cook, package orchestration |
| `SDE_ROADMAP.md` | Deterministic data compiler, canonical IR, artifact hashes, dependency manifests |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md` | Gameplay graph artifacts, block registry, graph compile and validation |
| `AUTHORING_AUTHORITY_MODEL.md` | Authority, replication, persistence, prediction, security validation |
| `SAGA_SCRIPTING_ROADMAP.md` | C# script compile, block-callable bindings, generated C#, hot reload boundaries |
| `ASSET_PIPELINE_ROADMAP.md` | Source asset import, cook, asset manifests, runtime-ready artifacts |
| `PRISM_ROADMAP.md` | Stale artifact detection, boundary validation, generated code origin analysis |
| `ENGINE_ROADMAP.md` | Runtime/server consumption of artifacts and manifests |
| `EDITOR_ROADMAP.md` | Editor-facing build/diagnostics UX |
| `COLLABORATION_ROADMAP.md` | Collaboration locks, conflicts, publish-blocking state |
| `DIAGNOSTICS_ROADMAP.md` | Structured diagnostics and reports |
| `SHARED_ROADMAP.md` | Shared artifact/diagnostic/package contracts |
| `DependencyGraph.md` | Dependency and ownership boundaries |

---

## 3. Pipeline Ownership

### 3.1 Saga Product Ownership

- [ ] `Saga` owns product-facing build and publish workflow.

  Done means Saga can:

  - show build/publish actions,
  - select build/publish profiles,
  - initiate validation/build/package/publish checks,
  - show current pipeline state,
  - display diagnostics and publish blockers,
  - route users to affected graph/script/asset/source locations,
  - preserve product-level failure state.

- [ ] `Saga` does not own build internals.

  Done means Saga delegates planning/execution to Forge and consumes reports, diagnostics, and artifacts.

---

### 3.2 Forge Ownership

- [ ] Forge owns build plan generation and build step orchestration.

  Done means Forge can:

  - resolve workspace,
  - select build profile,
  - create deterministic build plan,
  - run validation steps,
  - invoke SDE,
  - invoke script compiler,
  - invoke asset cook tools,
  - invoke package steps,
  - aggregate diagnostics,
  - emit build reports,
  - return stable exit codes.

- [ ] Forge does not own SDE, script compiler, asset cooker, Prism, runtime, or product shell internals.

---

### 3.3 SDE Ownership

- [ ] SDE owns validation and compilation of SDE source and graph definitions.

  Done means SDE emits:

  - canonical IR,
  - compiled graph artifacts,
  - artifact hashes,
  - dependency manifests,
  - source maps,
  - diagnostics.

---

### 3.4 Scripting Ownership

- [ ] Script toolchain owns script compile and binding manifest emission.

  Done means script compilation emits:

  - script assemblies,
  - script artifact manifests,
  - block binding manifests,
  - authority metadata,
  - source maps,
  - diagnostics.

---

### 3.5 Asset Pipeline Ownership

- [ ] Asset pipeline owns source asset import/cook artifact generation.

  Done means asset pipeline emits:

  - imported metadata,
  - cooked artifacts,
  - asset manifests,
  - dependency manifests,
  - cook diagnostics,
  - budget data.

---

### 3.6 Prism Ownership

- [ ] Prism owns static analysis and stale artifact checks.

  Done means Prism can validate:

  - generated code freshness,
  - cooked artifact freshness,
  - graph/script/asset artifact relationships,
  - dependency boundary violations,
  - package manifest consistency where supported.

---

### 3.7 Runtime/Server Ownership

- [ ] Runtime/server consume packaged artifacts and manifests.

  Done means runtime/server startup validates:

  - package manifest version,
  - asset manifest version,
  - graph artifact versions,
  - script binding manifests,
  - authority artifact placement,
  - required runtime/server artifacts.

---

## 4. Build Profiles

Required build profiles:

```txt
editor-preview
dev-client
dev-server
dev-local-client-server
test
ci
shipping-client
shipping-server
shipping-full
```

---

### 4.1 editor-preview

- [ ] Define editor-preview profile.

  Done means editor-preview builds:

  - fast SDE outputs,
  - graph artifacts for editor/runtime preview,
  - generated code previews,
  - script artifacts where needed,
  - editor-preview cooked assets,
  - diagnostics optimized for authoring feedback.

---

### 4.2 dev-client

- [ ] Define dev-client profile.

  Done means dev-client builds:

  - client runtime package,
  - client-safe graph artifacts,
  - client script artifacts,
  - client asset manifests,
  - debug diagnostics metadata.

---

### 4.3 dev-server

- [ ] Define dev-server profile.

  Done means dev-server builds:

  - server runtime package,
  - server-only graph artifacts,
  - authoritative script artifacts,
  - server data manifests,
  - server diagnostics metadata.

---

### 4.4 dev-local-client-server

- [ ] Define local client/server preview profile.

  Done means local preview builds matching client and server artifacts together and validates compatibility before preview launch.

---

### 4.5 shipping-client / shipping-server / shipping-full

- [ ] Define shipping profiles.

  Done means shipping builds:

  - stricter validation,
  - no editor-only artifacts,
  - no test-only artifacts,
  - no stale generated outputs,
  - restricted script/security profile,
  - complete package manifests,
  - publish readiness reports.

Shipping profile is where optimism goes to be denied by evidence.

---

## 5. Build Plan Model

- [ ] Define deterministic build plan.

  Done means a build plan contains:

  - build profile,
  - target platform,
  - ordered steps,
  - step dependencies,
  - expected inputs,
  - expected outputs,
  - cache keys,
  - tool invocations,
  - diagnostics policy,
  - failure policy.

- [ ] Define build step kinds.

  Required step kinds:

```txt
ResolveWorkspace
ValidateProjectManifest
ValidateToolchain
ValidateCollaborationState
ValidateSDE
CompileSDE
ValidateGameplayGraph
ValidateAuthority
GenerateGraphCode
ValidateScripts
CompileScripts
ValidateAssets
CookAssets
RunPrismBoundaryChecks
RunPrismStaleChecks
StageClientPackage
StageServerPackage
GeneratePackageManifests
ValidatePackageManifests
RunPublishReadinessCheck
WriteBuildReport
```

- [ ] Reject invalid step graphs.

  Done means Forge detects:

  - missing inputs,
  - missing outputs,
  - dependency cycles,
  - duplicate artifact writers,
  - invalid step ordering,
  - incompatible profile/target combinations.

---

## 6. Workspace and Project Validation

- [ ] Validate workspace before expensive steps.

  Done means pipeline checks:

  - workspace root exists,
  - project manifest exists,
  - required directories exist or can be created,
  - generated/build/package roots are safe,
  - toolchain is available,
  - profile exists,
  - required tools are available.

- [ ] Validate project manifest.

  Required checks:

```txt
project id valid
display name valid
project version supported
SDE root valid
asset roots valid
script roots valid
generated root valid
build root valid
package root valid
default profiles valid
```

- [ ] Fail with actionable diagnostics.

  Example:

```txt
Project manifest references SDE root `.sde`, but the directory does not exist.
Create the directory or update `saga.project.json`.
```

---

## 7. Collaboration Gate

- [ ] Validate collaboration state before publish-sensitive workflows.

  Done means build/publish can detect:

  - unresolved conflicts,
  - hard locks on publish-relevant resources,
  - pending schema migration,
  - dirty resources not included in build,
  - disconnected owner of critical lock,
  - insufficient publish permission.

- [ ] Do not block normal local validation unnecessarily.

  Done means quick local validation can run in the presence of collaboration issues, but publish/package profiles can enforce stricter gates.

---

## 8. SDE Pipeline

- [ ] Add SDE validation step.

  Done means Forge invokes SDE validate and collects diagnostics without emitting production artifacts unless requested.

- [ ] Add SDE compile step.

  Done means Forge invokes SDE compile and consumes:

  - canonical IR,
  - graph artifacts,
  - schema artifacts,
  - dependency manifests,
  - source maps,
  - artifact hashes,
  - diagnostics.

- [ ] Block partial artifact publication on SDE failure.

  Done means failed SDE compile cannot publish fake valid artifact state.

- [ ] Preserve deterministic output.

  Done means identical input state produces identical artifact hashes and manifests where practical.

---

## 9. Gameplay Graph and Authority Pipeline

- [ ] Validate gameplay graph artifacts.

  Done means pipeline checks:

  - graph syntax/semantic validity,
  - block references,
  - pin compatibility,
  - graph schema versions,
  - block registry compatibility,
  - source maps,
  - graph artifact hash.

- [ ] Validate authority metadata.

  Done means pipeline rejects:

  - server-only blocks in client graphs,
  - client-only artifacts staged into server authority as truth,
  - server-only artifacts staged as executable client logic,
  - prediction-unsafe behavior in predicted graphs,
  - persistent writes without server authority,
  - editor-only blocks in runtime artifacts.

- [ ] Generate graph code where profile requires it.

  Done means graph-to-C# or graph-to-runtime binding outputs are generated with source maps and freshness hashes.

---

## 10. Script Pipeline

- [ ] Validate script projects.

  Done means pipeline checks:

  - script manifest validity,
  - source roots,
  - references,
  - target domains,
  - generated source freshness,
  - allowed APIs by profile.

- [ ] Compile scripts.

  Done means script compile emits:

  - assemblies,
  - binding manifests,
  - authority metadata,
  - source maps,
  - diagnostics,
  - artifact hashes.

- [ ] Validate block-callable bindings.

  Done means pipeline rejects:

  - missing authority metadata,
  - invalid signatures,
  - unsupported parameter types,
  - stale binding manifests,
  - server-only APIs in client package,
  - editor/test-only APIs in shipping package.

---

## 11. Asset Pipeline

- [ ] Validate asset metadata.

  Done means pipeline checks:

  - stable asset ids,
  - source asset hashes,
  - import settings,
  - cook settings,
  - dependency graph,
  - missing references,
  - platform/profile compatibility.

- [ ] Cook assets.

  Done means asset cook emits:

  - runtime-ready artifacts,
  - asset manifests,
  - dependency manifests,
  - cook diagnostics,
  - budget metadata,
  - artifact hashes.

- [ ] Reject stale cooked artifacts.

  Done means stale source/import/cook state fails build/package profiles that require fresh artifacts.

---

## 12. Prism Analysis Pipeline

- [ ] Run boundary validation.

  Done means Prism can report architecture violations such as:

```txt
Engine includes Editor
Server includes editor-private headers
SDE depends on Saga modules
Forge depends on SDE internals
Runtime includes SDE parser internals
```

- [ ] Run stale artifact validation.

  Done means Prism can detect stale:

  - generated C#,
  - graph artifacts,
  - script binding manifests,
  - cooked asset artifacts,
  - package manifests where supported.

- [ ] Run relationship checks.

  Done means Prism can connect:

  - graph source → generated code,
  - script source → binding manifest,
  - source asset → cooked artifact,
  - SDE definition → asset reference,
  - package manifest → staged artifact.

---

## 13. Package Staging

- [ ] Define package staging layout.

  Required package outputs:

```txt
Packages/editor-preview/
Packages/dev-client/
Packages/dev-server/
Packages/shipping-client/
Packages/shipping-server/
```

- [ ] Define client package contents.

  Client package may contain:

  - client runtime manifest,
  - client-safe graph artifacts,
  - client script artifacts,
  - client asset artifacts,
  - UI/presentation data,
  - replication read/apply descriptors,
  - diagnostics metadata allowed for profile.

- [ ] Define server package contents.

  Server package may contain:

  - server runtime manifest,
  - authoritative graph artifacts,
  - server script artifacts,
  - server data artifacts,
  - validation rules,
  - persistence schema refs,
  - server diagnostics metadata.

- [ ] Reject invalid package placement.

  Done means package staging fails when:

  - server-only executable logic enters client package,
  - editor-only artifacts enter shipping runtime package,
  - test-only artifacts enter shipping package,
  - package misses required artifact,
  - manifest hash disagrees with staged artifact.

---

## 14. Manifest Generation

- [ ] Generate build manifest.

  Done means each build emits:

  - build id,
  - profile,
  - target platform,
  - tool versions,
  - input hashes,
  - step outputs,
  - diagnostics summary,
  - artifact list.

- [ ] Generate artifact manifest.

  Done means artifact manifest includes:

  - artifact id,
  - artifact kind,
  - source ref,
  - output path,
  - content hash,
  - profile,
  - target platform,
  - schema/format version,
  - dependencies.

- [ ] Generate package manifest.

  Done means package manifest includes:

  - package id,
  - package kind,
  - profile,
  - runtime compatibility,
  - artifact refs,
  - asset manifest refs,
  - script manifest refs,
  - graph manifest refs,
  - diagnostics summary,
  - package hash.

---

## 15. Publish Readiness Check

- [ ] Add publish readiness model.

  Done means publish readiness can be:

```txt
Ready
ReadyWithWarnings
Blocked
Failed
Unknown
```

- [ ] Define publish blockers.

  Required blocker categories:

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

- [ ] Produce publish report.

  Done means publish readiness emits:

  - status,
  - blockers,
  - warnings,
  - affected resources,
  - diagnostics refs,
  - suggested actions,
  - package refs where ready.

Example report summary:

```txt
Publish blocked:
- AuthorityValidationError: QuestReward graph uses ServerOnly block in ClientOnly artifact
- StaleArtifact: terrain_albedo texture cooked from old source hash
- ScriptCompileError: InventoryRules.cs has 2 compile errors
```

---

## 16. Diagnostics Aggregation

- [ ] Aggregate diagnostics from all pipeline steps.

  Required sources:

```txt
Project validation
Collaboration gate
SDE
Gameplay graph validation
Authority validation
Script compiler/analyzer
Asset cook
Prism checks
Package staging
Runtime/server manifest validation
```

- [ ] Emit machine-readable diagnostic report.

  Required format:

```txt
JSON primary
human-readable summary secondary
```

- [ ] Preserve source locations.

  Done means diagnostics can navigate to:

  - project manifest,
  - SDE source,
  - graph node/pin/edge,
  - C# source range,
  - asset source/metadata,
  - Forge step,
  - Prism report item,
  - package manifest entry.

---

## 17. Cache and Incrementality

- [ ] Define cache keys per pipeline step.

  Cache keys must include:

  - input content hashes,
  - tool version,
  - compiler/cooker version,
  - profile,
  - target platform,
  - dependency hashes,
  - schema versions,
  - relevant build options.

- [ ] Prevent unsafe cache reuse.

  Done means stale cache hits are rejected when:

  - source hash changed,
  - dependency hash changed,
  - tool version changed,
  - profile changed,
  - authority metadata changed,
  - package destination changed.

- [ ] Report cache decisions.

  Done means build reports can explain cache hits/misses for important steps.

---

## 18. CI and Automation

- [ ] Add CI-friendly commands.

  Required command concepts:

```txt
forge validate --profile ci
forge build --profile test
forge build --profile shipping-client
forge build --profile shipping-server
forge publish-check --profile shipping-full
prism validate-boundaries
prism stale
sde validate
```

- [ ] Use stable exit codes.

  Done means CI can distinguish:

  - success,
  - validation failure,
  - compile failure,
  - toolchain failure,
  - internal error,
  - publish blocked.

- [ ] Emit reports as artifacts.

  Required reports:

```txt
build_report.json
diagnostics_report.json
publish_report.json
artifact_manifest.json
package_manifest.json
```

---

## 19. Editor/Product UX

### 19.1 Product Build View

- [ ] Add product build view.

  Done means Saga can display:

  - selected profile,
  - build steps,
  - current step,
  - elapsed time,
  - diagnostics count,
  - artifact output summary,
  - failure state.

---

### 19.2 Publish Readiness View

- [ ] Add publish readiness view.

  Done means Saga can display:

  - ready/blocked state,
  - blocker list,
  - affected resources,
  - suggested actions,
  - rerun check action,
  - package output paths when ready.

---

### 19.3 Editor Problems Integration

- [ ] Route pipeline diagnostics to editor Problems panel.

  Done means editor users can click diagnostics and navigate to exact source resources.

---

## 20. Runtime/Server Startup Validation

- [ ] Runtime validates package manifests on startup.

  Done means runtime checks:

  - package manifest version,
  - asset manifest availability,
  - client graph/script artifact compatibility,
  - artifact hashes where configured,
  - profile compatibility.

- [ ] Server validates package manifests on startup.

  Done means server checks:

  - server package kind,
  - authoritative graph artifacts,
  - server script artifacts,
  - persistence/schema refs,
  - authority manifests,
  - artifact hashes,
  - package compatibility.

- [ ] Startup failure is explicit.

  Done means runtime/server do not continue with missing or invalid required package state.

---

## 21. Testing Strategy

### 21.1 Build Plan Tests

- [ ] Add build plan tests.

  Required coverage:

  - deterministic step ordering,
  - missing input detection,
  - duplicate output detection,
  - dependency cycle detection,
  - invalid profile rejection,
  - correct profile step inclusion.

---

### 21.2 SDE/Graph/Authority Pipeline Tests

- [ ] Add data/graph validation tests.

  Required coverage:

  - valid graph build,
  - invalid SDE source fails,
  - invalid authority graph fails,
  - graph artifact hash emitted,
  - generated code source map emitted,
  - failed compile blocks package.

---

### 21.3 Script Pipeline Tests

- [ ] Add script pipeline tests.

  Required coverage:

  - valid script compile,
  - compile error fails build,
  - invalid block-callable binding fails,
  - stale generated C# detected,
  - server-only script rejected from client package.

---

### 21.4 Asset Pipeline Tests

- [ ] Add asset pipeline tests.

  Required coverage:

  - valid texture cook,
  - stale cooked artifact fails,
  - missing asset reference fails,
  - package manifest includes asset refs,
  - runtime manifest can load cooked asset refs.

---

### 21.5 Package Tests

- [ ] Add package staging tests.

  Required coverage:

  - client package staged correctly,
  - server package staged correctly,
  - editor-only artifact rejected from shipping package,
  - test-only artifact rejected from shipping package,
  - manifest hash mismatch fails.

---

### 21.6 Publish Check Tests

- [ ] Add publish readiness tests.

  Required coverage:

  - ready project passes,
  - graph error blocks,
  - authority error blocks,
  - script error blocks,
  - asset stale state blocks,
  - collaboration conflict blocks,
  - invalid package manifest blocks.

---

## 22. MVP Vertical Slice

The first build/publish vertical slice should validate a small MMO starter project.

Required scenario:

```txt
MMO Starter project with:
- one SDE quest definition
- one quest reward gameplay graph
- one C# pure helper block
- one imported texture asset
- one client package
- one server package
```

Required pipeline behavior:

1. Forge resolves workspace.
2. Project manifest validates.
3. SDE validates and compiles quest/graph definitions.
4. Authority validation confirms quest reward graph is ServerOnly.
5. C# helper script compiles and emits block binding manifest.
6. Texture asset cooks into runtime-ready artifact.
7. Prism stale checks pass.
8. Client package stages client-safe artifacts and texture.
9. Server package stages authoritative graph/script/data artifacts.
10. Package manifests are generated.
11. Publish readiness check returns `Ready`.
12. Changing texture source without recook produces `StaleArtifact` blocker.
13. Moving server-only graph artifact into client package produces `AuthorityValidationError` blocker.

This slice proves the pipeline is real.

Not complete.

Real.

---

## 23. Non-Goals

The build/publish pipeline must not become:

- a replacement for SDE compiler internals,
- a replacement for the asset importer/cooker implementation,
- a replacement for script compiler internals,
- a product shell,
- a runtime/server launcher that ignores package validity,
- a collection of undocumented shell scripts,
- a system that ships stale artifacts because build was “mostly fine”,
- a CI-hostile manual workflow.

The goal is not to build everything.

The goal is to know exactly what was built, why it is valid, and why publishing is allowed or blocked.

---

## 24. Risk Register

### 24.1 Risk: Pipeline Becomes Too Big Before Vertical Slice Works

Mitigation:

- start with MMO starter vertical slice,
- use minimal texture/script/graph examples,
- require reports and diagnostics early,
- avoid broad asset/language support before pipeline shape is proven.

---

### 24.2 Risk: Forge Absorbs Tool Internals

Mitigation:

- Forge invokes tools through boundaries,
- SDE/script/asset/Prism internals remain owned by their modules,
- build steps consume outputs/manifests, not private internals.

---

### 24.3 Risk: Stale Artifacts Ship

Mitigation:

- content hashes everywhere,
- Prism stale checks,
- Forge package validation,
- runtime/server startup manifest validation,
- publish blockers.

---

### 24.4 Risk: Product UX Hides Build Truth

Mitigation:

- Saga shows exact blockers,
- diagnostics remain source-linked,
- technical details are expandable,
- CI reports match product diagnostics.

---

### 24.5 Risk: Client/Server Packages Drift

Mitigation:

- package compatibility checks,
- shared package manifest schema,
- explicit client/server artifact destination metadata,
- preview profile validates both packages together.

---

## 25. Suggested File Targets

Expected Forge files:

```txt
Tools/Forge/include/Forge/Pipeline/BuildPipeline.hpp
Tools/Forge/include/Forge/Pipeline/BuildPlan.hpp
Tools/Forge/include/Forge/Pipeline/BuildStep.hpp
Tools/Forge/include/Forge/Pipeline/BuildProfile.hpp
Tools/Forge/include/Forge/Pipeline/BuildReport.hpp
Tools/Forge/include/Forge/Pipeline/PublishReadiness.hpp
Tools/Forge/include/Forge/Pipeline/PublishReport.hpp
Tools/Forge/src/Pipeline/BuildPipeline.cpp
Tools/Forge/src/Pipeline/BuildPlanner.cpp
Tools/Forge/src/Pipeline/PublishReadiness.cpp
```

Expected Forge step files:

```txt
Tools/Forge/include/Forge/Steps/ProjectValidateStep.hpp
Tools/Forge/include/Forge/Steps/SdeValidateStep.hpp
Tools/Forge/include/Forge/Steps/SdeCompileStep.hpp
Tools/Forge/include/Forge/Steps/GraphValidateStep.hpp
Tools/Forge/include/Forge/Steps/AuthorityValidateStep.hpp
Tools/Forge/include/Forge/Steps/ScriptCompileStep.hpp
Tools/Forge/include/Forge/Steps/AssetCookStep.hpp
Tools/Forge/include/Forge/Steps/PrismStaleCheckStep.hpp
Tools/Forge/include/Forge/Steps/PackageStageStep.hpp
Tools/Forge/include/Forge/Steps/ManifestGenerateStep.hpp
Tools/Forge/src/Steps/
```

Expected shared contracts:

```txt
Shared/include/SagaShared/Build/BuildProfile.hpp
Shared/include/SagaShared/Build/BuildStatus.hpp
Shared/include/SagaShared/Build/BuildDiagnostic.hpp
Shared/include/SagaShared/Build/BuildReport.hpp
Shared/include/SagaShared/Artifacts/ArtifactManifest.hpp
Shared/include/SagaShared/Packages/PackageManifest.hpp
Shared/include/SagaShared/Publish/PublishReadiness.hpp
Shared/include/SagaShared/Publish/PublishBlocker.hpp
Shared/include/SagaShared/Publish/PublishReport.hpp
```

Expected Saga product files:

```txt
Saga/include/Saga/Build/BuildWorkflowService.h
Saga/include/Saga/Build/BuildStatusPresenter.h
Saga/include/Saga/Publish/PublishWorkflowService.h
Saga/include/Saga/Publish/PublishReadinessPresenter.h
Saga/src/Build/BuildWorkflowService.cpp
Saga/src/Publish/PublishWorkflowService.cpp
```

Expected reports:

```txt
<Project>/Build/Reports/build_report.json
<Project>/Build/Reports/diagnostics_report.json
<Project>/Build/Reports/publish_report.json
<Project>/Build/Manifests/artifact_manifest.json
<Project>/Build/Manifests/package_manifest.client.json
<Project>/Build/Manifests/package_manifest.server.json
```

---

## 26. Decision Summary

Preserve these decisions:

```txt
Saga owns product-facing build/publish workflow.
Forge owns build planning and orchestration.
SDE owns deterministic data/graph compilation.
Script toolchain owns script compilation and binding manifests.
Asset pipeline owns cooked runtime-ready asset artifacts.
Prism owns stale/boundary/artifact relationship analysis.
Runtime/server consume packaged artifacts and validate manifests on startup.
Publish readiness must understand graph, authority, scripts, assets, packages, collaboration, and diagnostics.
Stale artifacts must be publish-blocking where required.
Client and server packages must have explicit artifact boundaries.
CI and product UI must report the same underlying truth.
```

The pipeline should make publishing boring:

```txt
Validate everything.
Build what changed.
Reject stale outputs.
Stage explicit artifacts.
Generate manifests.
Explain blockers.
Publish only when the project is actually ready.
```

If Saga can do that, the project stops being a collection of impressive parts and becomes a production workflow.
