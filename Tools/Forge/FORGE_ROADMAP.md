# Forge — Build Workflow Frontend Roadmap



---

## 0. Roadmap Convention



A build workflow frontend should know what to run, when to run it, what artifacts to expect, and why failure happened.

It should not become every tool it invokes.

---

## 1. Document Purpose

This document defines the roadmap for Forge.



Forge owns:



Forge does not own:



Correct model:

```txt
Forge
  reads project/build manifests
  lowers to BuildModel / BuildPlan
  invokes owned adapters or external tools
  validates expected inputs/outputs
  aggregates diagnostics/reports
  stages packages
  runs publish readiness checks
```

Incorrect model:



That is not integration.

That is toolchain hoarding.

---

## 2. Companion Documents

| Document                            | Purpose                                                                   |
| ----------------------------------- | ------------------------------------------------------------------------- |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md` | Full validate/compile/cook/analyze/package/publish pipeline               |
| `SAGA_PRODUCT_ROADMAP.md`           | Saga product shell that initiates and displays build/publish workflows    |
| `SDE_ROADMAP.md`                    | Standalone deterministic compiler invoked by Forge                        |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md`    | Graph artifacts and graph validation expectations                         |
| `AUTHORING_AUTHORITY_MODEL.md`      | Authority/persistence/replication/prediction validation metadata          |
| `SAGA_SCRIPTING_ROADMAP.md`         | Script compile, block bindings, generated C# artifacts, scripting reports |
| `ASSET_PIPELINE_ROADMAP.md`         | Source asset import/cook/artifact/manifest pipeline                       |
| `PRISM_ROADMAP.md`                  | Stale artifact and boundary analysis invoked/consumed by Forge            |
| `SHARED_ROADMAP.md`                 | Shared build/artifact/package/publish report contracts                    |
| `ENGINE_ROADMAP.md`                 | Runtime/server package and manifest consumption expectations              |
| `EDITOR_ROADMAP.md`                 | Editor build/publish status UX consuming Forge reports                    |
| `TOOLS_ROADMAP.md`                  | Tool ecosystem ownership index                                            |
| `DependencyGraph.md`                | Dependency and ownership boundaries                                       |
| `forge.toml — Schema Reference`     | Current Forge manifest schema                                             |
| `Forge CHANGELOG.md`                | Current shipped command behavior and implementation decisions             |

---

## 3. Current Shipped Baseline

* [x] Add internal build model.

  Represented by:

  ```txt
  BuildModel
  forge.toml lowering
  CLI override application after lowering
  ```

  Preserved decision:

  ```txt
  Forge should lower manifests into backend-neutral build state before adapters see commands.
  ```

* [x] Add backend adapter pattern.

  Represented by:

  ```txt
  CMakeAdapter
  ConanAdapter
  ```

  Preserved decision:

  ```txt
  Only backend adapters invoke cmake/ctest/conan directly.
  Other Forge code should not spawn those tools ad hoc.
  ```

* [x] Add `ToolEnv` executable resolution.

  Represented by:

  ```txt
  ToolEnv
  .forge env-overrides
  ToolEnv::Active()
  ```

  Preserved decision:

  ```txt
  Adapters use resolved tool executables instead of hardcoded binary names.
  ```

* [x] Add `EnvProbe`.

  Represented by:

  ```txt
  cmake version probing
  conan version probing
  compiler version probing
  forge env
  --strict enforcement
  ```

  Preserved decision:

  ```txt
  Forge should validate actual toolchain state before strict builds.
  ```

* [x] Add build/test/install/presets/env/fmt commands.

  Represented by:

  ```txt
  forge configure
  forge build
  forge clean
  forge test
  forge install-target
  forge plan
  forge presets
  forge env
  forge fmt
  --explain
  ```

* [x] Add strict mode and manifest validation basics.

  Represented by:

  ```txt
  --strict
  unknown top-level section rejection
  toolchain pin checking
  ```

* [x] Add `forge.lock` after successful install.

  Represented by:

  ```txt
  forge.lock
  install mode
  declared dependencies
  planned --strict --frozen verification
  ```

* [x] Add NixOS-specific fast-fail behavior.

  Represented by:

  ```txt
  project toolchain commands fail fast outside nix-shell
  explicit forge nix <command> path
  ```

---

## 4. Fundamental Ownership Rule

* [ ] Keep Forge a workflow orchestrator, not a universal implementation owner.

  Done means Forge can:



  But Forge does not:



* [ ] Treat every non-build subsystem as an external step or adapter boundary.

  Done means new integrations enter Forge as:

  * command adapter,
  * service adapter,
  * manifest reader,
  * report reader,
  * validation step,
  * package staging step.

Not as random includes from another subsystem’s private implementation.

---

## 5. Dependency Rules

### 5.1 Allowed Dependencies

Allowed dependency directions:



---

### 5.2 Forbidden Dependencies

Forbidden dependency directions:



Forbidden shortcuts:



---

## 6. Command Surface Roadmap

### 6.1 Existing Core Commands

* [x] Preserve existing Forge core commands.

  Required existing commands:

  ```txt
  forge new
  forge init
  forge add
  forge install
  forge configure
  forge build
  forge clean
  forge test
  forge install-target
  forge presets
  forge env
  forge fmt
  forge run
  ```

* [ ] Normalize command behavior.

  Done means all commands consistently support:

  * `--manifest` where applicable,
  * `--project` / working directory semantics where applicable,
  * `--strict` where applicable,
  * `--frozen` where applicable,
  * `--json` output where applicable,
  * `--explain` where applicable,
  * structured diagnostics where applicable,
  * stable exit codes.

---

### 6.2 New Saga Pipeline Commands

* [ ] Add `forge validate`.



  Expected usage:

  ```txt
  forge validate --profile editor-preview
  forge validate --profile ci
  forge validate --json
  ```



  Done means Forge can invoke:



  through configured tool boundaries and collect reports.

* [ ] Add `forge graph` validation/build step surface where useful.



* [ ] Add `forge scripts` or script compile step surface.

  Done means Forge can validate/compile scripts through a script compiler adapter boundary.

* [ ] Add `forge assets` or asset cook step surface.

  Done means Forge can validate/cook assets through asset pipeline tools/services.

* [ ] Add `forge package`.

  Done means Forge can stage client/server/editor-preview packages from built artifacts.

* [ ] Add `forge publish-check`.

  Done means Forge can generate publish readiness reports without necessarily uploading/deploying anything.

* [ ] Add `forge report`.

  Done means users/CI can inspect latest build/publish/diagnostics reports.

  `0.0.8-dev.6` shipped a minimal build report reader preview:

  ```txt
  forge report --json
  forge report --build=DIR --json
  forge report --path=FILE --json
  ```

  Text mode only prints report path, file size, and a `--json` hint. Parsed summaries, schema validation, diagnostics reports, publish reports, and package reports remain open.

* [ ] Add `forge doctor`.

  Done means Forge checks:



---

## 7. Manifest Roadmap

### 7.1 Existing `forge.toml`

* [x] Keep `forge.toml` as project build manifest for Forge.

  Current supported sections:

  ```txt
  [project]
  [toolchain]
  [build]
  [deps]
  .forge env-overrides
  forge.lock
  ```

  Preserved decision:

  ```txt
  forge.toml is not a build language.
  CMake stays in CMake.
  Forge coordinates workflow and tool constraints.
  ```

* [ ] Keep unknown section behavior strict but forward-compatible.

  Done means unknown sections are preserved where appropriate but rejected under strict profiles when unsupported.

---

### 7.2 Saga Project Integration

* [ ] Add support for Saga project manifest discovery.

  Done means Forge can locate and consume:



* [ ] Avoid duplicating product manifest truth.

  Done means Forge reads Saga project/build profile metadata but does not become the product manifest owner.

---

### 7.3 Build Profiles

* [ ] Add build profile model.

  Required profiles:

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

* [ ] Add profile-specific strictness.

  Done means profiles can configure:



---

### 7.4 Future Manifest Sections

Possible future `forge.toml` or Saga build-profile sections:



[scripts]
root = "Scripts"
out = "Build/Artifacts/Scripts"

[assets]
root = "Assets"
out = "Build/Artifacts/Assets"

[package]
out = "Packages"

[publish]
profile = "shipping-full"
```

* [ ] Add only after roadmap/API stabilization.

  Done means schema expansion happens after build/publish concepts are implemented enough to avoid permanent manifest junk.

Manifests are hard to un-break once users copy them into every project.

Do not casually invent fields because a prototype needed a string.

---

## 8. Build Model and Build Plan

* [x] Keep `BuildModel` backend-neutral.

  Preserved decision:

  ```txt
  Manifest lowering happens before backend adapters.
  CLI overrides apply after lowering.
  ```

* [x] Add `BuildPlan` separate from `BuildModel`.

  Done means Forge distinguishes:

  * what project/build configuration says (`BuildModel`),
  * what steps will actually run (`BuildPlan`),
  * what happened (`BuildReport`).

Expected files:

```txt
Tools/Forge/include/Forge/Pipeline/BuildPlan.hpp
Tools/Forge/include/Forge/Pipeline/BuildPlanner.hpp
Tools/Forge/src/Pipeline/BuildPlanner.cpp
Tools/Forge/tests/BuildPlanTests.cpp
```

  Shipped in `0.0.8-dev.6` as a metadata-only planning foundation:

  ```txt
  BuildModel remains the existing manifest/CLI-lowered request model.
  BuildPlan now describes planned command steps.
  BuildReport foundation now describes planned/blocked BuildPlan reports.
  `forge plan --write-report` can persist planned/blocked preview reports.
  Command execution reports remain future work.
  ```

* [ ] Add build step dependency graph.

  Done means steps declare:

  * step id,
  * step kind,
  * inputs,
  * outputs,
  * dependencies,
  * cache key,
  * tool invocation,
  * expected reports,
  * failure policy.

* [ ] Detect invalid build plans.

  Done means Forge rejects:

  * dependency cycles,
  * missing required inputs,
  * duplicate output writers,
  * invalid profile/target combinations,
  * missing tool adapters,
  * unsupported step kind.

  `0.0.8-dev.6` shipped the foundation checks for duplicate step ids, missing dependencies, dependency cycles, and duplicate output writers. Profile/target validation, required input validation, missing tool adapter validation, and unsupported future step validation remain open.

---

## 9. Step Kinds

Required build step kinds:

```txt
ResolveWorkspace
ValidateProjectManifest
ValidateForgeManifest
ValidateToolchain
ValidateCollaborationState
InstallDependencies
ConfigureBackend
BuildBackend
RunTests
InstallTarget
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
StageEditorPreviewPackage
GenerateArtifactManifest
GeneratePackageManifest
ValidatePackageManifest
RunPublishReadinessCheck
WriteBuildReport
```

* [ ] Implement each step as an explicit unit.

  Done means each step has:

  * typed inputs,
  * typed outputs,
  * diagnostics,
  * exit behavior,
  * report contribution,
  * test coverage.

Expected files:

```txt
Tools/Forge/include/Forge/Steps/ProjectValidateStep.hpp
Tools/Forge/include/Forge/Steps/SdeValidateStep.hpp
Tools/Forge/include/Forge/Steps/SdeCompileStep.hpp
Tools/Forge/include/Forge/Steps/GraphValidateStep.hpp
Tools/Forge/include/Forge/Steps/AuthorityValidateStep.hpp
Tools/Forge/include/Forge/Steps/ScriptValidateStep.hpp
Tools/Forge/include/Forge/Steps/ScriptCompileStep.hpp
Tools/Forge/include/Forge/Steps/AssetValidateStep.hpp
Tools/Forge/include/Forge/Steps/AssetCookStep.hpp
Tools/Forge/include/Forge/Steps/PrismBoundaryCheckStep.hpp
Tools/Forge/include/Forge/Steps/PrismStaleCheckStep.hpp
Tools/Forge/include/Forge/Steps/PackageStageStep.hpp
Tools/Forge/include/Forge/Steps/ManifestGenerateStep.hpp
Tools/Forge/include/Forge/Steps/PublishReadinessStep.hpp
```

---

## 10. Backend Adapter Roadmap

### 10.1 CMake Adapter

* [x] Keep CMake invocation isolated in `CMakeAdapter`.

  Preserved decision:

  ```txt
  CMakeAdapter is the only Forge code path that invokes cmake/ctest for CMake operations.
  ```

* [ ] Expand adapter reports.

  Done means CMakeAdapter returns structured results including:

  * command line,
  * exit code,
  * stdout/stderr summary,
  * generated paths,
  * diagnostics where parseable,
  * timing.

---

### 10.2 Conan Adapter

* [x] Keep Conan invocation isolated in `ConanAdapter`.

  Preserved decision:

  ```txt
  ConanAdapter owns conan install invocation modes.
  ```

* [ ] Complete `--strict --frozen` behavior.

  Done means Forge validates:

  * `forge.lock` exists,
  * install mode matches,
  * dependency declarations match lock,
  * profiles match lock where applicable,
  * frozen install refuses drift.

---

### 10.3 Future Backend Adapters

* [ ] Define adapter interface for future build backends.

  Done means backends can be added without changing high-level pipeline logic.

Potential future adapters:

```txt
Ninja direct adapter
MSBuild adapter
Cargo adapter for Rust tools
Dotnet adapter for C# scripting
Custom Saga package adapter
```

Do not add backends before there is a real use case.

Backend collection is not product maturity.

---

## 11. Tool Environment and Toolchain

* [x] Use `ToolEnv` for executable resolution.

  Preserved decision:

  ```txt
  ToolEnv resolves tool executables once and adapters consume ToolEnv.
  ```

* [x] Use `.forge` env-overrides.

  Preserved decision:

  ```txt
  Project-local tool executable overrides are supported.
  ```

* [ ] Expand `ToolEnv` for Saga pipeline tools.

  Done means ToolEnv can resolve:



* [ ] Expand `EnvProbe` for Saga pipeline tools.

  Done means `forge env --json` can report:



* [ ] Add profile-level tool requirements.

  Done means shipping profiles can require stricter tool/version matches than dev profiles.

---







  * executable path from ToolEnv,
  * configured source root,
  * configured output root,
  * profile-specific options,
  * JSON diagnostics output,
  * artifact/dependency manifest outputs.

Expected files:

```txt
Tools/Forge/include/Forge/Adapters/SdeAdapter.hpp
Tools/Forge/src/Adapters/SdeAdapter.cpp
Tools/Forge/include/Forge/Steps/SdeValidateStep.hpp
Tools/Forge/include/Forge/Steps/SdeCompileStep.hpp
Tools/Forge/src/Steps/SdeValidateStep.cpp
Tools/Forge/src/Steps/SdeCompileStep.cpp
```







  Done means Forge reads:

  * diagnostics JSON,
  * artifact manifest,
  * dependency manifest,
  * source maps,
  * generated output manifests.



  Done means:



---

## 13. Gameplay Graph and Authority Integration

* [ ] Add graph validation step.



  * graph artifact presence,
  * graph schema version,
  * block registry compatibility,
  * source map presence,
  * graph artifact hash.

* [ ] Add authority validation step.

  Done means Forge validates:

  * graph authority metadata,
  * script authority metadata,
  * package artifact destination,
  * client/server artifact split,
  * prediction safety metadata,
  * persistence/replication effects.

Expected files:

```txt
Tools/Forge/include/Forge/Steps/GraphValidateStep.hpp
Tools/Forge/include/Forge/Steps/AuthorityValidateStep.hpp
Tools/Forge/src/Steps/GraphValidateStep.cpp
Tools/Forge/src/Steps/AuthorityValidateStep.cpp
```

* [ ] Keep authority implementation ownership clean.

  Done means Forge validates using manifests/descriptors and profile policies. It does not include server authority internals.

Example failure:

```txt
AuthorityValidationError:
ServerOnly graph artifact QuestReward was staged as executable client artifact.
```

---

## 14. Script Integration

* [ ] Add script compiler adapter.

  Done means Forge can invoke script compiler/toolchain through adapter boundary.

Expected files:

```txt
Tools/Forge/include/Forge/Adapters/ScriptCompilerAdapter.hpp
Tools/Forge/src/Adapters/ScriptCompilerAdapter.cpp
Tools/Forge/include/Forge/Steps/ScriptValidateStep.hpp
Tools/Forge/include/Forge/Steps/ScriptCompileStep.hpp
Tools/Forge/src/Steps/ScriptValidateStep.cpp
Tools/Forge/src/Steps/ScriptCompileStep.cpp
```

* [ ] Consume script outputs.

  Done means Forge reads:

  * script compile diagnostics,
  * script assembly artifacts,
  * binding manifests,
  * authority metadata,
  * generated code manifests,
  * source maps.

* [ ] Validate script package placement.

  Done means Forge rejects:

  * server-only script artifacts in client executable package,
  * editor-only scripts in shipping packages,
  * test-only scripts in shipping packages,
  * missing binding manifest,
  * stale generated code where policy requires freshness.

* [ ] Keep script compiler implementation outside Forge.

  Done means Forge does not become a C# compiler or runtime host.

---

## 15. Asset Pipeline Integration

* [ ] Add asset pipeline adapter.

  Done means Forge can invoke asset validation/cook through tool/service boundary.

Expected files:

```txt
Tools/Forge/include/Forge/Adapters/AssetPipelineAdapter.hpp
Tools/Forge/src/Adapters/AssetPipelineAdapter.cpp
Tools/Forge/include/Forge/Steps/AssetValidateStep.hpp
Tools/Forge/include/Forge/Steps/AssetCookStep.hpp
Tools/Forge/src/Steps/AssetValidateStep.cpp
Tools/Forge/src/Steps/AssetCookStep.cpp
```

* [ ] Consume asset outputs.

  Done means Forge reads:

  * asset diagnostics,
  * cooked artifact descriptors,
  * asset manifests,
  * dependency manifests,
  * budget data,
  * artifact hashes.

* [ ] Validate stale cooked assets.

  Done means Forge rejects stale asset artifacts when profile requires fresh cook outputs.

* [ ] Keep importer/cooker implementation outside Forge.

  Done means Forge does not implement texture/mesh/audio import/cook logic internally.

---

## 16. Prism Integration

* [ ] Add Prism adapter.

  Done means Forge can invoke Prism checks:

  * boundary validation,
  * stale artifact validation,
  * generated code origin validation,
  * package relationship validation where supported.

Expected files:

```txt
Tools/Forge/include/Forge/Adapters/PrismAdapter.hpp
Tools/Forge/src/Adapters/PrismAdapter.cpp
Tools/Forge/include/Forge/Steps/PrismBoundaryCheckStep.hpp
Tools/Forge/include/Forge/Steps/PrismStaleCheckStep.hpp
Tools/Forge/src/Steps/PrismBoundaryCheckStep.cpp
Tools/Forge/src/Steps/PrismStaleCheckStep.cpp
```

* [ ] Consume Prism reports.

  Done means Forge reads:

  * stale generated code reports,
  * stale cooked artifact reports,
  * dependency boundary reports,
  * generated origin reports,
  * package relationship reports.

* [ ] Keep Prism implementation outside Forge.

  Done means Forge does not index source, own symbol graphs, or inspect internal Prism databases.

---

## 17. Package Staging

* [ ] Add package staging model.

  Done means Forge can stage:

  * editor-preview packages,
  * dev-client packages,
  * dev-server packages,
  * shipping-client packages,
  * shipping-server packages,
  * shipping-full package sets.

Expected files:

```txt
Tools/Forge/include/Forge/Package/PackageKind.hpp
Tools/Forge/include/Forge/Package/PackageStagePlan.hpp
Tools/Forge/include/Forge/Package/PackageStager.hpp
Tools/Forge/src/Package/PackageStager.cpp
```

* [ ] Define client package staging rules.

  Client package may contain:

  * client runtime executable/assets,
  * client-safe graph artifacts,
  * client-safe script artifacts,
  * asset manifests,
  * cooked client asset artifacts,
  * client package manifest,
  * allowed diagnostics metadata.

* [ ] Define server package staging rules.

  Server package may contain:

  * server executable/assets,
  * authoritative graph artifacts,
  * server script artifacts,
  * server data artifacts,
  * persistence/schema refs,
  * server package manifest,
  * server diagnostics metadata.

* [ ] Reject invalid package placement.

  Done means Forge rejects:

  * server-only executable logic in client package,
  * editor-only artifacts in shipping package,
  * test-only artifacts in shipping package,
  * stale artifacts,
  * missing required artifacts,
  * manifest hash mismatches.

---

## 18. Manifest Generation

* [ ] Generate artifact manifests.

  Done means Forge emits manifests describing:

  * artifact id,
  * artifact kind,
  * source ref,
  * output path,
  * content hash,
  * profile,
  * target platform,
  * schema/format version,
  * dependencies.

Expected files:

```txt
Tools/Forge/include/Forge/Manifests/ArtifactManifestWriter.hpp
Tools/Forge/src/Manifests/ArtifactManifestWriter.cpp
```

* [ ] Generate package manifests.

  Done means Forge emits manifests describing:

  * package id,
  * package kind,
  * build profile,
  * runtime compatibility,
  * artifact refs,
  * asset manifest refs,
  * script manifest refs,
  * graph manifest refs,
  * diagnostics summary,
  * package hash.

Expected files:

```txt
Tools/Forge/include/Forge/Manifests/PackageManifestWriter.hpp
Tools/Forge/src/Manifests/PackageManifestWriter.cpp
```

* [ ] Generate build reports.

  Done means Forge emits:

  * build id,
  * profile,
  * target platform,
  * tool versions,
  * input hashes,
  * step results,
  * diagnostics summary,
  * artifact outputs,
  * cache decisions.

Expected files:

```txt
Tools/Forge/include/Forge/Reports/BuildReport.hpp
Tools/Forge/include/Forge/Reports/BuildReportBuilder.hpp
Tools/Forge/include/Forge/Reports/BuildReportWriter.hpp
Tools/Forge/src/Reports/BuildReportBuilder.cpp
Tools/Forge/src/Reports/BuildReportWriter.cpp
```

  `0.0.8-dev.6` shipped the Forge-local report foundation for BuildPlan validation reports:

  ```txt
  Tools/Forge/include/Forge/Reports/BuildReport.hpp
  Tools/Forge/include/Forge/Reports/BuildReportBuilder.hpp
  Tools/Forge/include/Forge/Reports/BuildReportWriter.hpp
  Tools/Forge/src/Reports/BuildReportBuilder.cpp
  Tools/Forge/src/Reports/BuildReportWriter.cpp
  Tools/Forge/tests/BuildReportTests.cpp
  ```

  `forge plan <command> [--json]` can preview these reports from the CLI without executing backend adapters.
  `forge plan <command> --write-report` can persist the preview to `<build-dir>/Reports/build_report.json`.
  `forge report --json` can stream persisted preview reports without parsing them.
  Tool versions, input hashes, real step execution results, artifact outputs, cache decisions, and diagnostics aggregation remain open.

---

## 19. Publish Readiness

* [ ] Add publish readiness step.

  Done means Forge can determine:

  ```txt
  Ready
  ReadyWithWarnings
  Blocked
  Failed
  Unknown
  ```

* [ ] Add publish blocker model.

  Required blocker kinds:

  ```txt
  ProjectManifestInvalid
  ToolchainInvalid
  SDECompileError
  GraphValidationError
  AuthorityValidationError
  ScriptCompileError
  AssetCookError
  StaleArtifact
  PackageManifestInvalid
  CollaborationConflict
  PermissionDenied
  RuntimeManifestInvalid
  ServerPackageInvalid
  DiagnosticsFatal
  ```

Expected files:

```txt
Tools/Forge/include/Forge/Publish/PublishReadiness.hpp
Tools/Forge/include/Forge/Publish/PublishBlocker.hpp
Tools/Forge/include/Forge/Publish/PublishReport.hpp
Tools/Forge/src/Publish/PublishReadiness.cpp
Tools/Forge/src/Publish/PublishReportWriter.cpp
```

* [ ] Emit publish report.

  Done means publish-check writes:

  ```txt
  <Project>/Build/Reports/publish_report.json
  ```

  with:

  * status,
  * blockers,
  * warnings,
  * affected resources,
  * diagnostics refs,
  * suggested actions,
  * package refs where ready.

---

## 20. Diagnostics and Reporting

* [ ] Add unified Forge diagnostic model.

  Done means Forge diagnostics include:

  * source system,
  * diagnostic code,
  * severity,
  * message,
  * resource ref,
  * artifact ref,
  * build step ref,
  * suggested action,
  * raw tool diagnostic where needed.

Expected files:

```txt
Tools/Forge/include/Forge/Diagnostics/ForgeDiagnostic.hpp
Tools/Forge/include/Forge/Diagnostics/DiagnosticCollector.hpp
Tools/Forge/src/Diagnostics/DiagnosticCollector.cpp
```

  `0.0.8-dev.6` shipped the Forge-local diagnostic collector foundation:

  ```txt
  Tools/Forge/include/Forge/Diagnostics/ForgeDiagnostic.hpp
  Tools/Forge/include/Forge/Diagnostics/DiagnosticCollector.hpp
  Tools/Forge/src/Diagnostics/DiagnosticCollector.cpp
  Tools/Forge/tests/DiagnosticCollectorTests.cpp
  ```

  BuildPlan validation diagnostics now flow through `DiagnosticCollector` before entering build reports. Source-system/resource/artifact refs, suggested actions, raw tool diagnostics, and cross-tool normalization remain open.

* [ ] Normalize tool diagnostics.



* [ ] Emit machine-readable reports.

  Required reports:

  ```txt
  build_report.json
  diagnostics_report.json
  artifact_manifest.json
  package_manifest.client.json
  package_manifest.server.json
  publish_report.json
  ```

* [ ] Keep human-readable summaries useful.

  Done means CLI output is concise but points to report files and primary blockers.

No one wants a 4,000-line terminal scroll as the only artifact of failure.

---

## 21. Cache and Incrementality

* [ ] Define per-step cache keys.

  Cache keys must include:

  * input content hashes,
  * tool version,
  * profile,
  * target platform,
  * dependency hashes,
  * schema versions,
  * compile/cook options,
  * package destination.

* [ ] Add safe incremental execution.

  Done means Forge can skip steps only when:

  * all inputs are unchanged,
  * tool versions match,
  * output manifests exist,
  * artifact hashes match,
  * profile/target match,
  * dependency manifests match.

* [ ] Reject unsafe cache hits.

  Done means stale/generated/cooked/package artifacts are not reused silently.

* [ ] Report cache decisions.

  Done means build reports explain important hits/misses/skips.

---

## 22. Strict, Frozen, and Lock Behavior

* [x] Implement basic strict mode toolchain checks.

  Preserved decision:

  ```txt
  --strict verifies toolchain pins.
  ```

* [ ] Complete frozen mode.

  Done means `--strict --frozen` rejects:

  * dependency drift,
  * missing lock file,
  * changed install mode,
  * changed profile host/build,
  * changed declared deps,
  * generated artifact drift where configured,
  * package manifest drift where configured.

* [ ] Extend lock model for Saga pipeline.

  Done means future lock files may record:



* [ ] Keep lock files human-inspectable.

  Done means lock data is structured and explainable, not binary mystery paste.

---

## 23. NixOS and Environment Behavior

* [x] Fail fast outside `nix-shell` on NixOS for project toolchain commands.

  Preserved decision:

  ```txt
  NixOS behavior should fail clearly instead of producing confusing toolchain errors.
  ```

* [ ] Expand `forge nix` behavior.

  Done means Forge can enter Nix shell for supported commands where configured.

* [ ] Document NixOS workflow.

  Done means users understand:

  * when to use `nix develop`,
  * when to use `forge nix <command>`,
  * how ToolEnv resolves commands inside/outside shell,
  * what fails under strict mode.

---

## 24. CI and Exit Codes

* [ ] Define stable exit codes.

  Required categories:

  ```txt
  Success
  ValidationFailure
  CompileFailure
  ToolchainFailure
  DependencyFailure
  PackageFailure
  PublishBlocked
  InternalError
  UserCancelled
  ```

  `0.0.8-dev.6` shipped the Forge-local foundation for named exit code categories:

  ```txt
  Tools/Forge/include/Forge/ExitCode.h
  Tools/Forge/src/ExitCode.cpp
  Tools/Forge/tests/ExitCodeTests.cpp
  ```

  Existing CLI numeric behavior is preserved for success, usage errors, execution failures,
  and strict failures. Full command classification, CI profile behavior, package failures,
  and publish-blocker exit behavior remain open.

* [ ] Make CI output reliable.

  Done means CI can run:

  ```txt
  forge validate --profile ci --json
  forge build --profile test --json
  forge build --profile shipping-client --json
  forge build --profile shipping-server --json
  forge publish-check --profile shipping-full --json
  ```

* [ ] Emit report paths on failure.

  Done means CI logs show where diagnostics/build/publish reports were written.

---

## 25. External Usability

Forge must remain useful beyond Saga-specific projects where practical.

* [ ] Preserve standalone C++ project workflow.

  Done means Forge continues to support:

  * `forge new`,
  * `forge init`,
  * `forge install`,
  * `forge configure`,
  * `forge build`,
  * `forge test`,
  * `forge fmt`,
  * CMake/Conan workflow without Saga project manifest.

* [ ] Make Saga pipeline features opt-in.



* [ ] Keep errors clear when Saga-only commands are used in non-Saga project.

  Example:

  ```txt
  forge publish-check requires a Saga project manifest or explicit package profile.
  ```

Forge can serve Saga without becoming unusable for smaller C++ projects.



---

## 26. Testing Strategy

### 26.1 Existing Command Tests

* [x] Preserve existing validation coverage.

  Existing examples:

  ```txt
  python3 -m py_compile Tools/scripts/export_tool_mirrors.py Tools/SagaTools/setup.py
  cargo check --manifest-path Tools/SagaTools/Cargo.toml
  python3 Tools/SagaTools/setup.py --no-smoke --no-symlink
  ./export.sh --dry-run --tool Prism
  ```

  Note: these are ecosystem checks, not complete Forge tests. Forge needs its own pipeline tests.

---

### 26.2 Build Model Tests

* [ ] Add BuildModel/BuildPlan tests.

  Required coverage:

  * manifest lowering,
  * CLI override application,
  * invalid profile rejection,
  * dependency cycle detection,
  * missing input detection,
  * duplicate output writer detection,
  * step ordering determinism.

  `0.0.8-dev.6` added standalone `forge_build_plan_tests` for command-to-step lowering, dependency cycle detection, missing dependency detection, duplicate output writer detection, duplicate step id detection, and generated plan validation. Manifest lowering, CLI override application, and invalid profile rejection remain open.

---

### 26.3 Adapter Tests

* [ ] Add adapter tests.

  Required coverage:

  * CMakeAdapter command construction,
  * ConanAdapter dispatch modes,
  * SdeAdapter command construction,
  * ScriptCompilerAdapter command construction,
  * AssetPipelineAdapter command construction,
  * PrismAdapter command construction,
  * ToolEnv override behavior,
  * `--explain` output.

---

### 26.4 Pipeline Step Tests

* [ ] Add step tests.

  Required coverage:



---

### 26.5 Report Tests

* [ ] Add report generation tests.

  Required coverage:

  * build report JSON,
  * diagnostics report JSON,
  * artifact manifest JSON,
  * package manifest JSON,
  * publish report JSON,
  * invalid report version rejection,
  * missing required fields.

  `0.0.8-dev.6` added `forge_build_report_tests` coverage for planned/blocked BuildPlan reports, validation issue diagnostics, deterministic JSON fields, and JSON string escaping. Full command execution reports and schema validation coverage remain open.
  CTest also covers `forge_diagnostic_collector_tests`, `forge plan build --json`, text `forge plan test`, unknown plan command failure, `forge plan --write-report` persistence smoke paths, and minimal `forge report` JSON/text/missing-file paths.

---

### 26.6 CI Tests

* [ ] Add CI profile tests.

  Required coverage:



---

## 27. MVP Vertical Slice

The first Forge production pipeline slice should use a minimal Saga project.

Required project contents:



Required workflow:

```txt
forge validate --profile editor-preview
forge build --profile dev-local-client-server
forge publish-check --profile shipping-full
```

Required behavior:



This proves Forge is not just a CMake wrapper anymore.

It becomes the build workflow frontend for a real Saga project.

---

## 28. Non-Goals

Forge must not become:



Forge coordinates.

It does not become every coordinated thing.

---

## 29. Risk Register

### 29.1 Risk: Forge Becomes Too Saga-Specific

Mitigation:

* keep C++ project workflow intact,
* make Saga pipeline features opt-in,
* separate Saga project manifest handling from core CMake/Conan flow,
* keep external usability tests.

---

### 29.2 Risk: Forge Absorbs Tool Internals

Mitigation:

* use adapters,
* consume reports/manifests,
* forbid private includes,
* enforce dependency boundaries.

---

### 29.3 Risk: Build Reports Are Too Vague

Mitigation:

* structured diagnostics,
* source/resource/artifact refs,
* exact blocker categories,
* report files as CI artifacts.

---

### 29.4 Risk: Stale Artifacts Ship

Mitigation:

* content hashes,
* dependency manifests,
* Prism stale checks,
* Forge package validation,
* publish readiness blockers.

---

### 29.5 Risk: Manifest Schema Expands Too Early

Mitigation:

* add schema fields only after pipeline behavior is real,
* preserve unknown sections carefully,
* strict mode rejects unsupported sections,
* document versioning.

---

## 30. Suggested File Targets

Expected pipeline files:

```txt
Tools/Forge/include/Forge/Pipeline/BuildPlan.hpp
Tools/Forge/include/Forge/Pipeline/BuildPlanner.hpp
Tools/Forge/include/Forge/Pipeline/BuildProfile.hpp
Tools/Forge/src/Pipeline/BuildPlanner.cpp
Tools/Forge/src/Pipeline/BuildPipeline.cpp
```

Shipped foundation files:

```txt
Tools/Forge/include/Forge/Pipeline/BuildPlan.hpp
Tools/Forge/include/Forge/Pipeline/BuildPlanner.hpp
Tools/Forge/src/Pipeline/BuildPlanner.cpp
Tools/Forge/tests/BuildPlanTests.cpp
```

Expected adapters:

```txt
Tools/Forge/include/Forge/Adapters/CMakeAdapter.hpp
Tools/Forge/include/Forge/Adapters/ConanAdapter.hpp
Tools/Forge/include/Forge/Adapters/SdeAdapter.hpp
Tools/Forge/include/Forge/Adapters/ScriptCompilerAdapter.hpp
Tools/Forge/include/Forge/Adapters/AssetPipelineAdapter.hpp
Tools/Forge/include/Forge/Adapters/PrismAdapter.hpp
Tools/Forge/src/Adapters/
```

Expected step files:

```txt
Tools/Forge/include/Forge/Steps/ProjectValidateStep.hpp
Tools/Forge/include/Forge/Steps/SdeValidateStep.hpp
Tools/Forge/include/Forge/Steps/SdeCompileStep.hpp
Tools/Forge/include/Forge/Steps/GraphValidateStep.hpp
Tools/Forge/include/Forge/Steps/AuthorityValidateStep.hpp
Tools/Forge/include/Forge/Steps/ScriptValidateStep.hpp
Tools/Forge/include/Forge/Steps/ScriptCompileStep.hpp
Tools/Forge/include/Forge/Steps/AssetValidateStep.hpp
Tools/Forge/include/Forge/Steps/AssetCookStep.hpp
Tools/Forge/include/Forge/Steps/PrismBoundaryCheckStep.hpp
Tools/Forge/include/Forge/Steps/PrismStaleCheckStep.hpp
Tools/Forge/include/Forge/Steps/PackageStageStep.hpp
Tools/Forge/include/Forge/Steps/ManifestGenerateStep.hpp
Tools/Forge/include/Forge/Steps/PublishReadinessStep.hpp
```

Expected report/package files:

```txt
Tools/Forge/include/Forge/Reports/BuildReport.hpp
Tools/Forge/include/Forge/Reports/BuildReportBuilder.hpp
Tools/Forge/include/Forge/Reports/BuildReportWriter.hpp
Tools/Forge/src/Reports/BuildReportBuilder.cpp
Tools/Forge/src/Reports/BuildReportWriter.cpp
Tools/Forge/include/Forge/Diagnostics/ForgeDiagnostic.hpp
Tools/Forge/include/Forge/Diagnostics/DiagnosticCollector.hpp
Tools/Forge/src/Diagnostics/DiagnosticCollector.cpp
Tools/Forge/include/Forge/Package/PackageStager.hpp
Tools/Forge/include/Forge/Manifests/ArtifactManifestWriter.hpp
Tools/Forge/include/Forge/Manifests/PackageManifestWriter.hpp
Tools/Forge/include/Forge/Publish/PublishReadiness.hpp
Tools/Forge/include/Forge/Publish/PublishReport.hpp
```

Shipped report foundation files:

```txt
Tools/Forge/include/Forge/Reports/BuildReport.hpp
Tools/Forge/include/Forge/Reports/BuildReportBuilder.hpp
Tools/Forge/include/Forge/Reports/BuildReportWriter.hpp
Tools/Forge/src/Reports/BuildReportBuilder.cpp
Tools/Forge/src/Reports/BuildReportWriter.cpp
Tools/Forge/tests/BuildReportTests.cpp
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

## 31. Decision Summary

Preserve these decisions:



The build pipeline should become boring in the best possible way:

```txt
Resolve project.
Validate inputs.
Run explicit steps.
Reject stale outputs.
Stage packages.
Write reports.
Explain blockers.
Exit correctly.
```

If Forge does that, Saga stops depending on rituals and starts having a production workflow.
