# Saga Build and Publish Pipeline Roadmap



---

## 0. Roadmap Convention



A build pipeline that only works when the developer remembers the correct ritual is not a build pipeline.

It is folklore with shell scripts.

---

## 1. Document Purpose

This document defines Saga's build and publish pipeline roadmap.

The build/publish pipeline connects the major Saga systems into one product-validating chain:



The target is a pipeline where a project can be checked and packaged reproducibly, with clear diagnostics and no silent stale artifact state.

Correct model:



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





---







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



- [ ] Fail with actionable diagnostics.

  Example:



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











  - canonical IR,
  - graph artifacts,
  - schema artifacts,
  - dependency manifests,
  - source maps,
  - artifact hashes,
  - diagnostics.





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



- [ ] Run stale artifact validation.

  Done means Prism can detect stale:

  - generated C#,
  - graph artifacts,
  - script binding manifests,
  - cooked asset artifacts,
  - package manifests where supported.

- [ ] Run relationship checks.

  Done means Prism can connect:



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

- [x] Add publish readiness model.

  Done means publish readiness can be:

```txt
Ready
ReadyWithWarnings
Blocked
Failed
Unknown
```

- [x] Define publish blockers.

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

  Shipped in `0.0.8-dev.6` as shared data-only publish readiness, blocker kind, blocker, and publish report contracts under `Shared/include/SagaShared/Publish/`.

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



- [ ] Emit machine-readable diagnostic report.

  Required format:

```txt
JSON primary
human-readable summary secondary
```

- [ ] Preserve source locations.

  Done means diagnostics can navigate to:



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



- [ ] Use stable exit codes.

  Done means CI can distinguish:

  - success,
  - validation failure,
  - compile failure,
  - toolchain failure,
  - internal error,
  - publish blocked.

  `0.0.8-dev.6` shipped a Forge-local named exit code foundation for success,
  usage errors, execution failures, strict failures, validation failures, toolchain
  failures, dependency failures, and internal errors. Full CI profile behavior,
  compile/package/publish classification, and publish-blocker exit behavior remain open.

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



- [ ] Add data/graph validation tests.

  Required coverage:



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



Required pipeline behavior:



This slice proves the pipeline is real.

Not complete.

Real.

---

## 23. Non-Goals

The build/publish pipeline must not become:



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
Shared/include/SagaShared/Build/BuildConfiguration.hpp
Shared/include/SagaShared/Build/BuildId.hpp
Shared/include/SagaShared/Build/BuildReport.hpp
Shared/include/SagaShared/Build/BuildStepKind.hpp
Shared/include/SagaShared/Build/BuildStepResult.hpp
Shared/include/SagaShared/Build/TargetPlatform.hpp
Shared/include/SagaShared/Artifacts/ArtifactManifest.hpp
Shared/include/SagaShared/Artifacts/ArtifactRef.hpp
Shared/include/SagaShared/Packages/PackageManifest.hpp
Shared/include/SagaShared/Packages/PackageValidationResult.hpp
Shared/include/SagaShared/Publish/PublishReadiness.hpp
Shared/include/SagaShared/Publish/PublishBlocker.hpp
Shared/include/SagaShared/Publish/PublishBlockerKind.hpp
Shared/include/SagaShared/Publish/PublishReport.hpp
```

Foundational shared contracts shipped in `0.0.8-dev.6`. Forge pipeline execution, manifest generation, report writers, diagnostics aggregation, JSON schemas, and product workflow services remain open.

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
