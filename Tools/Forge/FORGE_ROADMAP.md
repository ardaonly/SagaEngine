# Forge — Build Workflow Frontend Roadmap

> Last updated: 2026-05-14  
> Status: Active roadmap  
> Target: A production-grade build workflow frontend for Saga projects.  
> Scope: Workspace build orchestration, build profile selection, validation routing, SDE invocation, asset cook coordination, package build workflow, diagnostics aggregation, artifact staging, cache integration, and developer-facing build commands.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files, modules, or integration points that represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item describes the finished production state rather than interim scaffolding.
- Shipped items must name the files, modules, or integration points that represent the completed work.
- Open items must describe the finished state, not temporary scaffolding.
- Forge owns build workflow frontend behavior.
- Forge does not own the SDE compiler.
- Forge does not own Prism code intelligence.
- Forge does not own SagaTools dispatch.
- Forge does not own Saga product shell.
- Forge does not own runtime/server implementation.
- Forge may invoke tools and coordinate build steps through explicit interfaces.

---

## 1. Document Purpose

This document defines the roadmap for Forge.

Forge is Saga’s build workflow frontend.

It exists to provide a clear, repeatable, diagnosable build workflow for Saga projects.

Forge owns:

- build command UX,
- build profile selection,
- workspace build planning,
- validation step orchestration,
- SDE invocation as a compiler step,
- asset cook step coordination,
- artifact staging,
- package build workflow,
- build diagnostics aggregation,
- build cache integration,
- build summary output,
- CI-friendly build commands.

Forge does not own:

- SDE parser,
- SDE AST,
- SDE semantic analysis,
- SDE IR,
- SDE artifact generation internals,
- Prism indexing engine,
- Prism graph database,
- SagaTools dispatcher,
- Saga product shell,
- editor UI,
- runtime/server execution logic.

Correct model:

```txt
Forge
  plans and coordinates build workflow

SDE
  compiles deterministic data definitions

Asset pipeline
  imports/cooks asset artifacts

Runtime/server
  consume built artifacts

SagaTools
  may dispatch Forge commands
```

Incorrect model:

```txt
Forge becomes compiler, asset importer, project shell, code indexer, and runtime launcher.
```

That is not a build frontend.

That is five tools in a trench coat pretending to be architecture.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `docs/roadmaps/TOOLS_ROADMAP.md` | Tool ecosystem ownership index |
| `Tools/Forge/FORGE_ROADMAP.md` | Forge build workflow frontend roadmap |
| `Tools/SystemDefinitionEngine/SDE_ROADMAP.md` | SDE deterministic data compiler roadmap |
| `Tools/Prism/PRISM_ROADMAP.md` | Prism code intelligence roadmap |
| `Tools/SagaTools/SAGATOOLS_ROADMAP.md` | Thin tool dispatcher roadmap |
| `SHARED_ROADMAP.md` | Shared contracts and artifact references |
| `ENGINE_ROADMAP.md` | Runtime/server consumption of built artifacts |
| `EDITOR_ROADMAP.md` | Editor build/publish UX integration |
| `DependencyGraph.md` | Dependency ownership rules |

---

## 3. Ownership Boundary

- [x] Define Forge as build workflow frontend.

  Represented by:

  ```txt
  Tools/Forge/FORGE_ROADMAP.md
  docs/roadmaps/TOOLS_ROADMAP.md
  DependencyGraph.md
  ```

  Preserved decision:

  ```txt
  Forge coordinates build workflows.
  Forge does not own SDE, Prism, SagaTools, runtime, editor, or product shell implementation.
  ```

- [ ] Keep Forge focused on build orchestration.

  Done means Forge owns:

  - build command parsing,
  - build plan creation,
  - build profile selection,
  - tool step invocation,
  - validation routing,
  - artifact staging,
  - package build summary,
  - build diagnostics aggregation,
  - cache usage decisions,
  - CI-friendly output.

- [ ] Prevent Forge from owning tool internals.

  Done means Forge does not own:

  - SDE compiler AST,
  - SDE parser,
  - SDE optimizer,
  - SDE codegen,
  - Prism source indexing,
  - Prism symbol graph,
  - SagaTools command dispatch,
  - engine runtime systems,
  - editor UI panels,
  - server authority.

---

## 4. Dependency Rules

### 4.1 Allowed Dependencies

- [ ] Allow Forge to invoke standalone tools.

  Allowed tool invocations:

  ```txt
  Forge → SDE executable
  Forge → asset cook tool
  Forge → package tool
  Forge → validation tool
  Forge → Prism executable where explicitly needed for build metadata
  ```

- [ ] Allow Forge to consume stable shared contracts where explicitly approved.

  Allowed examples:

  ```txt
  Forge → package manifest contract
  Forge → artifact reference contract
  Forge → diagnostic payload contract
  Forge → build profile descriptor
  ```

- [ ] Allow SagaTools to dispatch Forge.

  Correct direction:

  ```txt
  SagaTools → Forge executable
  ```

---

### 4.2 Forbidden Dependencies

- [ ] Prevent Forge from depending on SDE internals.

  Forbidden:

  ```txt
  Forge → SDE parser internals
  Forge → SDE AST internals
  Forge → SDE semantic analyzer
  Forge → SDE IR internals
  Forge → SDE codegen internals
  ```

- [ ] Prevent Forge from depending on editor/runtime/server private implementation.

  Forbidden:

  ```txt
  Forge → SagaEditor UI
  Forge → SagaEngine runtime internals
  Forge → SagaServer private headers
  Forge → Saga product shell internals
  ```

- [ ] Prevent Forge from becoming SagaTools.

  Forbidden:

  ```txt
  Forge owns top-level tool dispatch for all tools
  Forge replaces sagatools list/doctor routing
  Forge becomes global tool registry
  ```

Forge builds.

SagaTools dispatches.

These are different verbs, and the fact that this must be written down is exactly why documentation exists.

---

## 5. CLI Roadmap

- [ ] Provide stable Forge CLI.

  Required commands:

  ```txt
  forge help
  forge version
  forge doctor
  forge build
  forge clean
  forge rebuild
  forge validate
  forge cook
  forge package
  forge inspect
  ```

- [ ] Provide `forge build`.

  Done means:

  - workspace is resolved,
  - build profile is selected,
  - build plan is created,
  - validation steps run,
  - SDE compile step runs where required,
  - asset cook steps run where required,
  - artifacts are staged,
  - build summary is emitted,
  - exit code reflects success/failure.

- [ ] Provide `forge validate`.

  Done means:

  - project manifest is validated,
  - package manifest is validated,
  - SDE validation is invoked where required,
  - asset metadata is validated,
  - build profile is validated,
  - diagnostics are aggregated.

- [ ] Provide `forge cook`.

  Done means:

  - asset cook plan is created,
  - source assets are mapped to output artifacts,
  - cook tools are invoked,
  - failed cook blocks build where required,
  - cook diagnostics are aggregated.

- [ ] Provide `forge package`.

  Done means:

  - built artifacts are collected,
  - package manifest is emitted,
  - runtime/package layout is created,
  - version metadata is included,
  - package diagnostics are emitted.

- [ ] Provide `forge clean`.

  Done means:

  - generated build artifacts can be removed,
  - cache can be preserved or cleared by flag,
  - output directories are validated before deletion,
  - destructive cleanup requires safe path checks.

---

## 6. Build Workspace Model

- [ ] Add workspace discovery.

  Done means Forge can resolve:

  - explicit workspace path,
  - current directory workspace,
  - project manifest,
  - package manifest,
  - build config,
  - output root.

Expected files:

```txt
Tools/Forge/include/Forge/Workspace/WorkspaceContext.hpp
Tools/Forge/include/Forge/Workspace/WorkspaceLocator.hpp
Tools/Forge/src/Workspace/WorkspaceLocator.cpp
```

- [ ] Add workspace validation.

  Done means Forge validates:

  - workspace root exists,
  - project manifest exists,
  - source directories exist,
  - output directory is writable,
  - required tools are available,
  - build profile exists.

- [ ] Keep product project lifecycle outside Forge.

  Done means Forge does not own:

  - project dashboard,
  - recent project registry,
  - product-level create/open workflow,
  - Saga mode switching.

Saga owns product lifecycle.

Forge builds what it is given.

A build tool that starts owning product UX is just a launcher having delusions of grandeur.

---

## 7. Build Profiles

- [ ] Add build profile model.

  Done means build profiles can describe:

  - profile name,
  - target platform,
  - build configuration,
  - output path,
  - validation strictness,
  - SDE compile options,
  - asset cook options,
  - package options,
  - cache policy.

Expected files:

```txt
Tools/Forge/include/Forge/Build/BuildProfile.hpp
Tools/Forge/include/Forge/Build/BuildConfiguration.hpp
Tools/Forge/include/Forge/Build/TargetPlatform.hpp
```

- [ ] Support common build profiles.

  Required profiles:

  ```txt
  dev
  debug
  release
  shipping
  server
  client
  editor-preview
  ```

- [ ] Validate build profile compatibility.

  Done means Forge rejects:

  - unknown platform,
  - invalid output path,
  - unsupported artifact format,
  - incompatible SDE artifact version,
  - incompatible asset cook format,
  - missing required build steps.

---

## 8. Build Plan

- [ ] Add build plan generation.

  Done means Forge can create an ordered build plan from workspace and profile.

Expected files:

```txt
Tools/Forge/include/Forge/Build/BuildPlan.hpp
Tools/Forge/include/Forge/Build/BuildStep.hpp
Tools/Forge/include/Forge/Build/BuildGraph.hpp
Tools/Forge/src/Build/BuildPlanner.cpp
```

- [ ] Define build step model.

  Done means each build step has:

  - step id,
  - display name,
  - step kind,
  - inputs,
  - outputs,
  - dependencies,
  - tool invocation,
  - cache key,
  - diagnostics,
  - failure policy.

- [ ] Add build graph validation.

  Done means Forge detects:

  - missing inputs,
  - missing outputs,
  - dependency cycles,
  - duplicate artifact outputs,
  - incompatible step ordering,
  - invalid tool references.

- [ ] Keep build plan deterministic.

  Done means identical workspace/profile inputs produce the same build step order and cache keys.

---

## 9. Build Step Categories

- [ ] Add manifest validation step.

  Done means Forge validates project/package manifests before expensive build steps run.

- [ ] Add SDE validation step.

  Done means Forge invokes SDE validation and collects diagnostics.

- [ ] Add SDE compile step.

  Done means Forge invokes SDE compile and consumes output artifact manifests.

- [ ] Add asset cook step.

  Done means Forge invokes asset cook tools and stages cooked outputs.

- [ ] Add script compile step where applicable.

  Done means script artifacts are compiled and diagnosed through the owning scripting toolchain.

- [ ] Add runtime package step.

  Done means runtime-consumable package layout is produced.

- [ ] Add server package step.

  Done means server-specific package layout can be produced where required.

- [ ] Add publish validation step.

  Done means shipping/publish builds verify all required gates before package success.

---

## 10. SDE Integration

- [ ] Invoke SDE as an external compiler step.

  Done means Forge can run:

  ```txt
  sde validate
  sde compile
  ```

  or equivalent configured executable paths.

- [ ] Consume SDE artifact manifests.

  Done means Forge can read:

  - artifact ids,
  - artifact kinds,
  - output paths,
  - schema versions,
  - content hashes,
  - dependency data,
  - diagnostics summary.

- [ ] Fail build on SDE compile failure.

  Done means:

  - SDE parse errors fail build,
  - SDE semantic errors fail build,
  - SDE artifact emission errors fail build,
  - SDE deterministic mismatch fails build.

- [ ] Preserve SDE ownership.

  Done means Forge does not include:

  ```txt
  Tools/SystemDefinitionEngine/src/**
  SDE AST internals
  SDE parser internals
  SDE IR internals
  SDE semantic internals
  ```

Forge runs the compiler.

Forge is not the compiler.

This is apparently a difficult spiritual concept for build tools.

---

## 11. Asset Cook Integration

- [ ] Add asset cook orchestration.

  Done means Forge can:

  - discover source assets,
  - read asset metadata,
  - create cook jobs,
  - invoke cook workers/tools,
  - collect cook diagnostics,
  - stage cooked artifacts.

Expected files:

```txt
Tools/Forge/include/Forge/Assets/AssetCookPlan.hpp
Tools/Forge/include/Forge/Assets/AssetCookStep.hpp
Tools/Forge/include/Forge/Assets/AssetCookResult.hpp
Tools/Forge/src/Assets/AssetCookPlanner.cpp
```

- [ ] Add asset dependency tracking.

  Done means Forge can track:

  - source asset path,
  - cooked artifact path,
  - dependency assets,
  - material/texture references,
  - generated metadata,
  - content hashes.

- [ ] Fail build on required asset cook failure.

  Done means:

  - failed cook emits diagnostics,
  - missing required cooked artifacts fail build,
  - invalid asset metadata fails build,
  - optional assets are handled by explicit policy.

- [ ] Keep editor import UX outside Forge.

  Done means Forge does not own:

  - drag-and-drop import UI,
  - asset inspector,
  - content browser,
  - artist-facing import workflow.

Forge cooks/builds.

Editor authors/imports.

Runtime streams cooked outputs.

See? Three separate jobs. Civilization trembles, but survives.

---

## 12. Artifact Staging

- [ ] Add artifact staging model.

  Done means Forge can stage:

  - SDE artifacts,
  - cooked assets,
  - script artifacts,
  - runtime manifests,
  - server manifests,
  - package metadata,
  - diagnostics summaries.

Expected files:

```txt
Tools/Forge/include/Forge/Artifacts/ArtifactStage.hpp
Tools/Forge/include/Forge/Artifacts/ArtifactStager.hpp
Tools/Forge/include/Forge/Artifacts/StagedArtifact.hpp
Tools/Forge/src/Artifacts/ArtifactStager.cpp
```

- [ ] Add artifact manifest output.

  Done means Forge emits a build artifact manifest containing:

  - build id,
  - profile,
  - target platform,
  - artifact list,
  - content hashes,
  - source references,
  - tool versions,
  - build timestamp or deterministic build id policy,
  - diagnostics summary.

- [ ] Add artifact integrity checks.

  Done means Forge verifies:

  - expected artifacts exist,
  - hash matches,
  - artifact version is compatible,
  - required artifacts are not missing,
  - duplicate artifact ids are rejected.

---

## 13. Build Cache

- [ ] Add build cache model.

  Done means Forge can cache outputs based on:

  - input file hashes,
  - tool versions,
  - build profile,
  - config values,
  - artifact format version,
  - target platform.

Expected files:

```txt
Tools/Forge/include/Forge/Cache/BuildCache.hpp
Tools/Forge/include/Forge/Cache/CacheKey.hpp
Tools/Forge/include/Forge/Cache/CacheEntry.hpp
Tools/Forge/src/Cache/BuildCache.cpp
```

- [ ] Add cache hit/miss diagnostics.

  Done means build summary shows:

  - cache hits,
  - cache misses,
  - invalidated steps,
  - cache errors,
  - cache size.

- [ ] Add cache invalidation policy.

  Done means cache invalidates when:

  - source input changes,
  - tool version changes,
  - profile changes,
  - schema/artifact format changes,
  - dependency changes,
  - config changes.

- [ ] Keep cache correctness above speed.

  Done means Forge never uses stale artifacts silently.

Fast wrong builds are still wrong builds.

They just waste your time with better posture.

---

## 14. Diagnostics Aggregation

- [ ] Add Forge diagnostic model.

  Done means diagnostics include:

  - severity,
  - code,
  - message,
  - source tool,
  - build step,
  - file/resource path,
  - recoverability,
  - suggested action.

Expected files:

```txt
Tools/Forge/include/Forge/Diagnostics/ForgeDiagnostic.hpp
Tools/Forge/include/Forge/Diagnostics/DiagnosticAggregator.hpp
Tools/Forge/src/Diagnostics/DiagnosticAggregator.cpp
```

- [ ] Aggregate child tool diagnostics.

  Done means Forge can collect diagnostics from:

  - SDE,
  - asset cook tools,
  - script compiler,
  - package validator,
  - internal build plan validator.

- [ ] Preserve diagnostic source ownership.

  Done means Forge reports whether a diagnostic came from:

  ```txt
  Forge
  SDE
  AssetCooker
  ScriptCompiler
  PackageValidator
  ```

- [ ] Support human-readable diagnostics.

  Done means CLI output is readable and grouped by build step.

- [ ] Support JSON diagnostics.

  Done means CI and editor integrations can consume structured build results.

---

## 15. Build Summary

- [ ] Add build summary output.

  Done means Forge reports:

  - build status,
  - selected profile,
  - target platform,
  - elapsed time,
  - step count,
  - failed steps,
  - warning count,
  - error count,
  - artifact count,
  - output path.

- [ ] Add machine-readable build summary.

  Done means `--json` emits stable summary data for CI/editor/product integration.

- [ ] Add build report artifact.

  Done means Forge can emit:

  ```txt
  build-report.json
  diagnostics.json
  artifact-manifest.json
  ```

---

## 16. Package Build

- [ ] Add package layout builder.

  Done means Forge can build package layouts for:

  - client runtime,
  - dedicated server,
  - editor preview,
  - development test package,
  - shipping package.

Expected files:

```txt
Tools/Forge/include/Forge/Package/PackageLayout.hpp
Tools/Forge/include/Forge/Package/PackageBuilder.hpp
Tools/Forge/include/Forge/Package/PackageManifestWriter.hpp
Tools/Forge/src/Package/PackageBuilder.cpp
```

- [ ] Add package manifest generation.

  Done means package manifest contains:

  - package id,
  - package version,
  - target platform,
  - build profile,
  - artifact list,
  - dependency list,
  - content hashes,
  - required runtime versions.

- [ ] Add package validation.

  Done means Forge rejects package when:

  - required artifact is missing,
  - incompatible artifact version exists,
  - required manifest is invalid,
  - package hash verification fails,
  - publish gate fails.

---

## 17. Publish Gate Integration

- [ ] Add build-level publish gates.

  Done means shipping/publish builds require:

  - SDE compile success,
  - asset cook success,
  - package validation success,
  - no unresolved build diagnostics above allowed severity,
  - artifact integrity verification,
  - target platform compatibility.

- [ ] Support collaboration publish gate input.

  Done means Forge can consume external publish state from product/collaboration services where provided, without owning collaboration implementation.

Allowed:

```txt
Forge consumes publish gate descriptor/artifact
Forge reports publish gate failure
```

Forbidden:

```txt
Forge owns collaboration session lifecycle
Forge owns permissions service
Forge owns conflict engine
```

---

## 18. Clean and Rebuild

- [ ] Add safe clean command.

  Done means `forge clean` can remove:

  - build output,
  - staged artifacts,
  - temporary files,
  - selected cache entries where requested.

- [ ] Add safe path validation before deletion.

  Done means Forge refuses to delete:

  - workspace root,
  - user home,
  - filesystem root,
  - paths outside configured build output unless explicitly forced with safety checks.

- [ ] Add rebuild command.

  Done means `forge rebuild` performs:

  ```txt
  clean selected outputs
  rebuild selected profile
  emit summary
  ```

Destructive build tools need adult supervision.

Forge should be the adult, not the toddler with `rm -rf`.

---

## 19. Configuration

- [ ] Add Forge config support.

  Done means config can define:

  - tool paths,
  - build profiles,
  - source roots,
  - output roots,
  - cache path,
  - target platforms,
  - artifact format preferences,
  - strictness levels.

Expected files:

```txt
Tools/Forge/include/Forge/Config/ForgeConfig.hpp
Tools/Forge/include/Forge/Config/ForgeConfigLoader.hpp
Tools/Forge/src/Config/ForgeConfigLoader.cpp
```

- [ ] Add command-line config overrides.

  Done means CLI can override:

  - workspace path,
  - profile,
  - output path,
  - cache mode,
  - verbosity,
  - JSON mode.

- [ ] Add config validation.

  Done means invalid config fails before build begins.

---

## 20. Tool Discovery

- [ ] Add required tool discovery.

  Done means Forge can locate:

  - SDE executable,
  - asset cook tool,
  - script compiler where applicable,
  - package validator,
  - optional Prism executable.

Expected files:

```txt
Tools/Forge/include/Forge/Tools/ToolLocator.hpp
Tools/Forge/include/Forge/Tools/ToolRequirement.hpp
Tools/Forge/include/Forge/Tools/ToolVersion.hpp
Tools/Forge/src/Tools/ToolLocator.cpp
```

- [ ] Add tool version validation.

  Done means Forge checks:

  - required tool exists,
  - executable can run,
  - version is compatible,
  - protocol/output format is supported.

- [ ] Add missing tool diagnostics.

  Done means missing tools report:

  - tool name,
  - expected path,
  - searched paths,
  - required version,
  - suggested install/build action.

---

## 21. Prism Integration

- [ ] Allow optional Prism integration for build insight.

  Done means Forge may invoke Prism for:

  - source graph inspection,
  - dependency insight,
  - stale generated code detection,
  - include/source graph diagnostics.

- [ ] Preserve Prism ownership.

  Done means Forge does not own:

  - Prism indexing,
  - Prism graph database,
  - symbol graph generation,
  - semantic query engine.

- [ ] Keep Prism optional for core build unless explicitly required by a profile.

  Done means normal build does not fail because Prism is unavailable unless the selected profile requires Prism.

---

## 22. SagaTools Integration

- [ ] Allow SagaTools to dispatch Forge.

  Example commands:

  ```txt
  sagatools forge build
  sagatools forge clean
  sagatools forge package
  sagatools forge doctor
  ```

- [ ] Keep SagaTools as dispatcher only.

  Done means SagaTools does not parse or execute Forge build plan internals.

- [ ] Keep Forge usable standalone.

  Done means Forge can still run as:

  ```txt
  forge build
  ```

  without requiring SagaTools.

---

## 23. Editor Integration

- [ ] Allow editor to invoke Forge through a service boundary.

  Done means SagaEditor can request:

  - validate project,
  - build current project,
  - cook assets,
  - package preview build,
  - inspect build diagnostics.

- [ ] Keep Forge UI outside Forge.

  Done means Forge does not own:

  - editor build panel,
  - progress dialog widgets,
  - Problems panel,
  - content browser,
  - product publish UI.

- [ ] Emit diagnostics consumable by editor.

  Done means Forge can output structured diagnostics that editor displays without importing Forge internals.

Correct flow:

```txt
Editor requests build
      ↓
Forge runs build workflow
      ↓
Forge emits diagnostics/build report
      ↓
Editor displays results
```

Incorrect flow:

```txt
Forge imports editor widgets and updates panels directly
```

No.

Bad boundary.

---

## 24. Runtime and Server Integration

- [ ] Emit runtime-consumable build artifacts.

  Done means SagaEngine runtime can consume:

  - asset manifests,
  - cooked assets,
  - SDE artifacts,
  - package manifest,
  - runtime config.

- [ ] Emit server-consumable build artifacts.

  Done means SagaServer can consume:

  - server package manifest,
  - authority-relevant schema artifacts,
  - runtime config,
  - validation metadata.

- [ ] Keep runtime/server implementation outside Forge.

  Done means Forge does not include runtime/server private headers or execute gameplay simulation.

Forge creates outputs.

Runtime/server consume outputs.

The oven bakes bread. It does not eat lunch.

---

## 25. Logging

- [ ] Add Forge logging.

  Done means logs include:

  - selected command,
  - workspace path,
  - build profile,
  - tool paths,
  - build steps,
  - step timings,
  - cache results,
  - artifact counts,
  - errors and warnings.

- [ ] Keep normal output readable.

  Done means default CLI output gives useful progress without drowning the user in raw internal noise.

- [ ] Add verbose mode.

  Done means `--verbose` shows detailed step/tool execution info.

---

## 26. Exit Codes

- [ ] Define stable Forge exit codes.

  Required categories:

  ```txt
  0   success
  1   general failure
  2   invalid arguments
  3   workspace error
  4   config error
  5   build plan error
  6   validation failure
  7   SDE failure
  8   asset cook failure
  9   package failure
  10  missing tool
  11  cache error
  12  internal error
  ```

- [ ] Preserve child tool failure classification.

  Done means an SDE failure is distinguishable from an asset cook failure, because “build failed” is technically true and practically useless.

---

## 27. Performance

- [ ] Add build timing metrics.

  Done means Forge records time for:

  - workspace discovery,
  - manifest validation,
  - build planning,
  - SDE step,
  - asset cook step,
  - package step,
  - artifact staging,
  - total build.

- [ ] Add slow step diagnostics.

  Done means slow build steps can be identified without guessing.

- [ ] Add incremental build performance tracking.

  Done means Forge reports:

  - skipped steps,
  - cache hits,
  - cache misses,
  - invalidated steps.

---

## 28. Testing Roadmap

### 28.1 Unit Tests

- [ ] Add workspace locator tests.

- [ ] Add config loader tests.

- [ ] Add build profile tests.

- [ ] Add build planner tests.

- [ ] Add build graph validation tests.

- [ ] Add cache key tests.

- [ ] Add artifact staging tests.

- [ ] Add diagnostics aggregation tests.

- [ ] Add safe clean path tests.

---

### 28.2 Integration Tests

- [ ] Add fake tool build integration test.

  Done means Forge can invoke fake SDE/cook/package tools and aggregate outputs.

- [ ] Add SDE integration smoke test.

  Done means Forge can invoke SDE validate/compile when available.

- [ ] Add asset cook integration smoke test.

  Done means Forge can invoke configured asset cook step when available.

- [ ] Add package build integration test.

  Done means staged artifacts produce package manifest.

- [ ] Add failure integration tests.

  Required cases:

  - SDE failure,
  - asset cook failure,
  - missing tool,
  - invalid config,
  - invalid workspace,
  - package validation failure.

---

### 28.3 Determinism Tests

- [ ] Add build plan determinism test.

  Done means identical workspace/profile produces identical build plan order.

- [ ] Add artifact manifest determinism test.

  Done means identical inputs produce identical artifact manifest excluding explicitly non-deterministic metadata.

- [ ] Add cache key determinism test.

  Done means cache keys are stable across repeated runs.

---

## 29. CI Requirements

- [ ] Add Forge unit tests to CI.

- [ ] Add Forge integration tests to CI.

- [ ] Add Forge CLI smoke tests.

  Required commands:

  ```txt
  forge --help
  forge version
  forge doctor
  forge validate --workspace <test-workspace>
  forge build --workspace <test-workspace> --profile dev
  ```

- [ ] Add dependency boundary checks.

  Required forbidden checks:

  ```txt
  Tools/Forge/** must not include Tools/SystemDefinitionEngine/src/**
  Tools/Forge/** must not include Tools/Prism/src/**
  Tools/Forge/** must not include Tools/SagaTools/src/**
  Tools/Forge/** must not include Editor/**
  Tools/Forge/** must not include Server/private/**
  Tools/Forge/** must not include Apps/Saga/**
  ```

- [ ] Add JSON output compatibility test.

  Done means build report JSON remains parseable and schema-stable.

---

## 30. Recommended File Layout

Recommended target layout:

```txt
Tools/Forge/
  FORGE_ROADMAP.md
  README.md
  CMakeLists.txt or Cargo.toml

Tools/Forge/docs/
  FORGE_CLI.md
  FORGE_BUILD_PROFILES.md
  FORGE_ARTIFACTS.md
  FORGE_DIAGNOSTICS.md

Tools/Forge/include/Forge/
  Forge.hpp
  ForgeCommand.hpp
  ForgeResult.hpp

Tools/Forge/include/Forge/Workspace/
  WorkspaceContext.hpp
  WorkspaceLocator.hpp

Tools/Forge/include/Forge/Build/
  BuildProfile.hpp
  BuildConfiguration.hpp
  TargetPlatform.hpp
  BuildPlan.hpp
  BuildStep.hpp
  BuildGraph.hpp
  BuildPlanner.hpp

Tools/Forge/include/Forge/Assets/
  AssetCookPlan.hpp
  AssetCookStep.hpp
  AssetCookResult.hpp

Tools/Forge/include/Forge/Artifacts/
  ArtifactStage.hpp
  ArtifactStager.hpp
  StagedArtifact.hpp

Tools/Forge/include/Forge/Cache/
  BuildCache.hpp
  CacheKey.hpp
  CacheEntry.hpp

Tools/Forge/include/Forge/Diagnostics/
  ForgeDiagnostic.hpp
  DiagnosticAggregator.hpp

Tools/Forge/include/Forge/Package/
  PackageLayout.hpp
  PackageBuilder.hpp
  PackageManifestWriter.hpp

Tools/Forge/include/Forge/Tools/
  ToolLocator.hpp
  ToolRequirement.hpp
  ToolVersion.hpp

Tools/Forge/include/Forge/Config/
  ForgeConfig.hpp
  ForgeConfigLoader.hpp

Tools/Forge/src/
  main.cpp or main.rs
  Workspace/
  Build/
  Assets/
  Artifacts/
  Cache/
  Diagnostics/
  Package/
  Tools/
  Config/

Tools/Forge/tests/
  WorkspaceLocatorTests.cpp
  BuildPlannerTests.cpp
  BuildGraphTests.cpp
  CacheKeyTests.cpp
  ArtifactStagerTests.cpp
  DiagnosticAggregatorTests.cpp
  IntegrationTests.cpp
```

This layout is illustrative.

The ownership boundary is not.

---

## 31. Migration Plan

- [ ] Remove compiler-specific implementation from Forge if present.

  Done means SDE internals live only in SDE.

- [ ] Remove code intelligence implementation from Forge if present.

  Done means Prism internals live only in Prism.

- [ ] Remove global tool dispatch behavior from Forge if present.

  Done means SagaTools owns top-level dispatch.

- [ ] Convert Forge to build workflow frontend.

  Done means Forge owns:

  ```txt
  build plan
  build steps
  diagnostics aggregation
  artifact staging
  package workflow
  cache integration
  ```

- [ ] Add explicit tool invocation boundaries.

  Done means Forge invokes tools by process/plugin boundary, not by importing their private internals.

- [ ] Add CI dependency checks.

---

## 32. Non-Goals

Forge does not own:

- SDE compiler implementation,
- Prism code intelligence implementation,
- SagaTools command dispatch,
- Saga product shell,
- editor UI,
- runtime simulation,
- server authority,
- collaboration sessions,
- collaboration permissions,
- asset authoring UI,
- package publishing backend,
- source indexing database.

Related ownership:

| Area | Owner |
|---|---|
| Build workflow frontend | `Forge` |
| Deterministic data compiler | `SDE` |
| Code intelligence | `Prism` |
| Tool dispatch | `SagaTools` |
| Product shell | `Saga` |
| Authoring UI | `SagaEditor` |
| Runtime/server systems | `SagaEngine` / `SagaServer` |
| Collaboration implementation | `SagaCollaboration` |

---

## 33. Production Definition of Done

- [ ] Forge has a stable CLI.

- [ ] Forge can discover and validate workspaces.

- [ ] Forge supports build profiles.

- [ ] Forge creates deterministic build plans.

- [ ] Forge invokes SDE as a compiler step without depending on SDE internals.

- [ ] Forge coordinates asset cook steps.

- [ ] Forge stages artifacts.

- [ ] Forge emits package manifests.

- [ ] Forge validates package integrity.

- [ ] Forge supports build cache with correct invalidation.

- [ ] Forge aggregates diagnostics from child tools.

- [ ] Forge supports human-readable and JSON output.

- [ ] Forge has stable exit codes.

- [ ] Forge supports safe clean/rebuild.

- [ ] Forge integrates with SagaTools as a dispatched tool.

- [ ] Forge can be invoked by editor/product workflows through service boundaries.

- [ ] CI tests CLI, build planning, diagnostics, and dependency boundaries.

---

## 34. Final Architecture Rule

Forge should remain:

```txt
a build workflow frontend,
not a compiler,
not a code intelligence engine,
not a product shell,
not an editor backend,
not a runtime,
not a server,
not a tool landfill.
```

It should know:

```txt
what to build,
which profile to use,
which steps are required,
which tools to invoke,
where artifacts go,
how failures are reported,
and whether the result is safe to package.
```

It should not know:

```txt
how SDE parses schemas,
how Prism indexes source,
how Saga opens projects,
how the editor draws panels,
how the runtime simulates entities,
or how the server owns authority.
```

Forge succeeds when it makes builds repeatable, visible, and boring.

Boring builds are good.

Exciting builds are usually broken in new and expensive ways.