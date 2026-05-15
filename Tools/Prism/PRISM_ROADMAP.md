# Prism — Code and Artifact Intelligence Roadmap



---

## 0. Roadmap Convention



Prism should answer:

```txt
What depends on what?
What generated this file?
Is this artifact stale?
Did a boundary rule break?
Does this package contain what it claims?
```

Prism should not answer:

```txt
Can I compile, cook, package, execute, and publish everything myself?
```

That is how analysis tools become confused build systems with worse error messages.

---

## 1. Document Purpose

This document defines the roadmap for Prism.



Prism owns:



Prism does not own:



Correct model:

```txt
Project source + generated files + manifests + reports
      ↓
Prism analysis
      ↓
relationship graph + diagnostics + reports
      ↓
Forge / SagaEditor / CI / Saga consume reports
```

Incorrect model:

```txt
Prism notices stale generated code, so Prism regenerates it and edits the build output.
```



Prism observes, verifies, and reports.

---

## 2. Companion Documents

| Document                            | Purpose                                                                                         |
| ----------------------------------- | ----------------------------------------------------------------------------------------------- |
| `TOOLS_ROADMAP.md`                  | Tool ecosystem ownership index                                                                  |
| `FORGE_ROADMAP.md`                  | Build workflow frontend that invokes Prism checks and consumes reports                          |
| `SDE_ROADMAP.md`                    | Standalone deterministic compiler that emits manifests/source maps Prism can read               |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md`    | Gameplay graph artifacts and generated code relationship model                                  |
| `AUTHORING_AUTHORITY_MODEL.md`      | Authority metadata relationships Prism can check for staleness/mismatch                         |
| `SAGA_SCRIPTING_ROADMAP.md`         | C# block binding, generated C# origin, script artifact relationship model                       |
| `ASSET_PIPELINE_ROADMAP.md`         | Source asset → cooked artifact relationship model                                               |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md` | Build/package/publish pipeline where Prism checks run as gates                                  |
| `SHARED_ROADMAP.md`                 | Shared artifact/diagnostic/package contracts used in reports                                    |
| `EDITOR_ROADMAP.md`                 | Editor panels consume Prism reports for navigation and diagnostics                              |
| `ENGINE_ROADMAP.md`                 | Runtime/server consume artifacts that Prism can validate for freshness/relationship consistency |
| `DependencyGraph.md`                | Architecture boundary rules Prism should validate                                               |
| `forge.toml — Schema Reference`     | Forge project/build manifest Prism may inspect                                                  |

---

## 3. Current Intended Position

* [x] Define Prism as a code/artifact intelligence tool, not compiler/build owner.

  Represented by:

  ```txt
  PRISM_ROADMAP.md
  TOOLS_ROADMAP.md
  FORGE_ROADMAP.md
  BUILD_PUBLISH_PIPELINE_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  Prism analyzes source and artifacts.
  Prism does not compile, build, cook, or execute.
  ```

* [ ] Keep Prism independently usable.

  Done means Prism can run as:

  * standalone CLI,
  * CI command,
  * Forge-invoked analysis step,
  * editor-consumed report generator,
  * developer local diagnostic tool.

* [ ] Keep Prism useful beyond Saga-specific code where possible.

  Done means Prism can still analyze general C++ include/dependency relationships and generated file freshness for non-Saga projects where configured.

---

## 4. Ownership Boundary

### 4.1 Prism Owns

Prism owns:



---

### 4.2 Prism Does Not Own

Prism must not own:



---

### 4.3 Correct Integration Model



---

## 5. Dependency Rules

### 5.1 Allowed Dependencies

Allowed dependency directions:



Prism may use parsing/indexing libraries if approved, but those are Prism implementation dependencies, not Saga module dependencies.

---

### 5.2 Forbidden Dependencies

Forbidden dependency directions:



Forbidden shortcuts:



---

## 6. CLI Roadmap

* [ ] Provide stable Prism command surface.

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

* [ ] Add `prism index`.

  Done means Prism indexes configured project sources and emits index summary/report.

* [ ] Add `prism deps`.

  Done means Prism emits include/dependency graphs for configured source roots.

* [ ] Add `prism boundaries`.

  Done means Prism validates architecture rules such as dependency direction and forbidden includes.

* [ ] Add `prism stale`.

  Done means Prism validates generated/cooked/compiled artifact freshness using manifests and hashes.

* [ ] Add `prism generated-origin`.

  Done means Prism can explain which source/generated artifact produced a generated file or artifact.

* [ ] Add `prism artifacts`.

  Done means Prism can inspect artifact manifests and list relationships.

* [ ] Add `prism packages`.

  Done means Prism can inspect package manifests for staged artifact consistency.

* [ ] Add `prism doctor`.

  Done means Prism checks:

  * project roots,
  * compile database availability,
  * source roots,
  * generated roots,
  * manifest paths,
  * report output directory,
  * configured boundary rules,
  * tool version.

* [ ] Support `--json` output.

  Done means every analysis command can emit machine-readable output for Forge/CI/editor integration.

---

## 7. Source Discovery

* [ ] Add project source discovery.

  Done means Prism can discover:



* [ ] Support configurable roots.

  Required roots:



* [ ] Support exclude rules.

  Done means Prism excludes:

  * build directories,
  * package output directories where not directly inspected,
  * external dependency cache,
  * generated directories unless explicitly included,
  * vendor directories unless configured.

* [ ] Support file hashing.

  Done means Prism records content hashes for source and generated outputs used in stale checks.

---

## 8. C++ Indexing

* [ ] Add C++ include graph indexing.

  Done means Prism can analyze:

  * `#include` directives,
  * include path resolution where possible,
  * include graph edges,
  * transitive include paths,
  * forbidden include relationships.

Expected files:

```txt
Tools/Prism/include/Prism/Cpp/CppIncludeIndexer.hpp
Tools/Prism/include/Prism/Cpp/CppIncludeGraph.hpp
Tools/Prism/src/Cpp/CppIncludeIndexer.cpp
Tools/Prism/src/Cpp/CppIncludeGraph.cpp
```

* [ ] Add compile database support.

  Done means Prism can read `compile_commands.json` where available to improve include resolution.

* [ ] Add C++ symbol indexing where practical.

  Done means Prism can extract or consume symbol information for:

  * classes,
  * functions,
  * namespaces,
  * includes,
  * references where feasible,
  * generated origin mapping where available.

* [ ] Keep indexing non-authoritative.

  Done means C++ compiler remains source of compile truth. Prism reports analysis; it does not replace compilation.

---

## 9. C# Indexing and Script Binding Analysis

* [ ] Add C# source discovery.

  Done means Prism discovers C# source files under configured script roots and generated output roots.

Expected files:

```txt
Tools/Prism/include/Prism/Scripting/CSharpSourceIndexer.hpp
Tools/Prism/include/Prism/Scripting/CSharpSymbolIndex.hpp
Tools/Prism/src/Scripting/CSharpSourceIndexer.cpp
```

* [ ] Add `[BlockCallable]` metadata detection.

  Done means Prism can index methods/classes annotated as block-callable where syntax support allows.

Expected files:

```txt
Tools/Prism/include/Prism/Scripting/BlockCallableIndex.hpp
Tools/Prism/src/Scripting/BlockCallableIndex.cpp
```

* [ ] Compare C# source against script binding manifests.

  Done means Prism can detect:

  * missing binding manifest entry,
  * stale binding manifest,
  * signature hash mismatch,
  * authority metadata mismatch,
  * generated code origin mismatch.

* [ ] Do not compile C#.

  Done means Prism does not replace the script compiler/analyzer. It reads source and manifests for relationship checks.

---





  Done means Prism can parse and index:

  * artifact id,
  * artifact kind,
  * source refs,
  * source hashes,
  * output paths,
  * artifact hashes,
  * dependency refs,
  * compiler version,
  * schema version.

Expected files:





  Done means Prism can map generated artifacts back to:





  Done means Prism can determine which source/schema files affect which outputs.



  Done means Prism reports when:

  * source hash changed,
  * schema package hash changed,
  * compiler version changed,
  * dependency hash changed,
  * output artifact missing,
  * artifact hash mismatch.





---

## 11. Gameplay Graph Relationship Analysis

* [ ] Index graph artifacts.

  Done means Prism can list:

  * graph id,
  * graph kind,
  * graph source file,
  * generated artifact path,
  * generated code path where applicable,
  * authority metadata ref,
  * package destination.

Expected files:

```txt
Tools/Prism/include/Prism/Graph/GraphArtifactIndex.hpp
Tools/Prism/include/Prism/Graph/GraphGeneratedOrigin.hpp
Tools/Prism/src/Graph/GraphArtifactIndex.cpp
```

* [ ] Track graph source to generated C# relationship.

  Done means Prism can answer:

  ```txt
  Which graph generated this C# file?
  Which node generated this generated code region?
  Is generated code stale relative to graph artifact hash?
  ```

* [ ] Detect stale graph artifacts.

  Done means Prism reports when graph source/schema/block registry metadata changed but compiled graph artifact was not regenerated.

* [ ] Detect graph/package destination mismatches.

  Done means Prism reports when manifest data indicates:

  * server-only graph staged into client package,
  * client-only graph used as server authority,
  * editor-only graph staged into runtime package,
  * missing graph artifact in package manifest.

Prism may report this.

Forge/server/runtime must enforce where appropriate.

---

## 12. Authority Metadata Analysis

* [ ] Index authority metadata from graph/script/artifact manifests.

  Done means Prism can read:

  * authority context,
  * execution domain,
  * side effects,
  * replication effects,
  * persistence effects,
  * prediction safety,
  * security boundary.

Expected files:

```txt
Tools/Prism/include/Prism/Authority/AuthorityMetadataIndex.hpp
Tools/Prism/include/Prism/Validation/AuthorityMetadataConsistencyCheck.hpp
Tools/Prism/src/Authority/AuthorityMetadataIndex.cpp
```

* [ ] Detect authority metadata mismatch.

  Done means Prism reports:

  * generated C# authority comment/manifest stale,
  * script binding authority metadata differs from source annotations,
  * package destination conflicts with artifact authority metadata,
  * generated graph code metadata differs from graph artifact manifest.

* [ ] Keep enforcement outside Prism.

  Done means Prism reports mismatches. Forge/build/publish/runtime/server enforce failure behavior.

---

## 13. Asset and Cooked Artifact Analysis

* [ ] Index source asset to cooked artifact relationships.

  Done means Prism can read asset/cook manifests and answer:

  * which source asset produced this cooked artifact,
  * which import settings were used,
  * which cook profile produced it,
  * which source hash it was cooked from,
  * which packages include it.

Expected files:

```txt
Tools/Prism/include/Prism/Assets/AssetArtifactIndex.hpp
Tools/Prism/include/Prism/Assets/AssetDependencyIndex.hpp
Tools/Prism/src/Assets/AssetArtifactIndex.cpp
```

* [ ] Detect stale cooked artifacts.

  Done means Prism reports when:

  * source asset hash changed,
  * import settings changed,
  * cook settings changed,
  * cooker version changed,
  * dependency changed,
  * cooked artifact missing,
  * artifact hash mismatch.

Expected files:

```txt
Tools/Prism/include/Prism/Validation/StaleCookedArtifactCheck.hpp
Tools/Prism/src/Validation/StaleCookedArtifactCheck.cpp
```

* [ ] Detect missing asset references.

  Done means Prism reports broken asset refs from:



* [ ] Do not cook assets.

  Done means Prism never becomes asset pipeline implementation.

---

## 14. Package Manifest Analysis

* [ ] Index package manifests.

  Done means Prism can read:

  * package id,
  * package kind,
  * build profile,
  * target platform,
  * artifact refs,
  * asset manifest refs,
  * script manifest refs,
  * graph manifest refs,
  * package hash.

Expected files:

```txt
Tools/Prism/include/Prism/Packages/PackageManifestIndex.hpp
Tools/Prism/include/Prism/Validation/PackageManifestConsistencyCheck.hpp
Tools/Prism/src/Packages/PackageManifestIndex.cpp
```

* [ ] Detect package consistency issues.

  Done means Prism reports:

  * missing staged artifact,
  * hash mismatch,
  * artifact listed but absent,
  * artifact present but not listed,
  * client/server package relationship mismatch,
  * package references stale artifact manifest,
  * package kind conflicts with artifact destination metadata.

* [ ] Support package relationship reports.

  Done means Prism can emit reports showing:



---

## 15. Dependency Boundary Validation

* [ ] Add boundary rule model.

  Done means Prism can load architecture rules such as:



Expected files:

```txt
Tools/Prism/include/Prism/Boundaries/BoundaryRule.hpp
Tools/Prism/include/Prism/Boundaries/BoundaryRuleSet.hpp
Tools/Prism/include/Prism/Validation/BoundaryValidator.hpp
Tools/Prism/src/Boundaries/BoundaryValidator.cpp
```

* [ ] Add dependency graph report.

  Done means Prism can emit:

  * include graph,
  * module dependency graph,
  * forbidden edge diagnostics,
  * transitive dependency path for violations.

* [ ] Add public header checks.

  Required checks:



* [ ] Add CI profile.

  Done means `prism boundaries --profile ci` can fail CI on architecture violations.

---

## 16. Generated Output Origin Tracking

* [ ] Add generated origin manifest reader.

  Done means Prism can read manifests that describe:

  * generated file path,
  * generator id,
  * generator version,
  * source artifact id,
  * source file refs,
  * source hashes,
  * output hash,
  * source map ref.

Expected files:

```txt
Tools/Prism/include/Prism/Generated/GeneratedOriginManifest.hpp
Tools/Prism/include/Prism/Generated/GeneratedOriginIndex.hpp
Tools/Prism/src/Generated/GeneratedOriginIndex.cpp
```

* [ ] Detect generated file modification.

  Done means Prism reports when generated files were manually edited or no longer match origin manifest hash.

* [ ] Map generated files back to sources.

  Done means Prism can map:



* [ ] Support editor navigation.



---

## 17. Report Model

* [ ] Define Prism report types.

  Required report types:

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

* [ ] Define common report fields.

  Required fields:

  ```txt
  prismVersion
  projectRoot
  generatedAt
  profile
  inputs
  diagnostics
  summary
  records
  ```

* [ ] Make reports deterministic where practical.

  Done means repeated analysis on unchanged inputs produces stable ordering and stable report content except timestamps where explicitly included.

* [ ] Support human-readable summaries.

  Done means CLI output reports key blockers and points to machine-readable report paths.

---

## 18. Forge Integration

* [ ] Support Forge-invoked Prism checks.

  Done means Forge can run:

  ```txt
  prism boundaries --profile ci --json
  prism stale --profile shipping-full --json
  prism packages --profile shipping-full --json
  ```

* [ ] Return stable exit codes.

  Required categories:

  ```txt
  Success
  AnalysisFailed
  BoundaryViolation
  StaleArtifact
  MissingArtifact
  PackageMismatch
  InternalError
  ```

* [ ] Emit Forge-consumable diagnostics.

  Done means Forge can merge Prism reports into build/publish reports without losing source/tool context.

* [ ] Keep build action outside Prism.

  Done means Forge decides whether a Prism diagnostic blocks build/publish according to profile policy.

---

## 19. Editor Integration

* [ ] Support editor report consumption.

  Done means SagaEditor can display Prism reports in:

  * Problems panel,
  * Diagnostics report panel,
  * generated code preview panel,
  * graph editor diagnostics,
  * asset inspector,
  * build/publish panel.

* [ ] Support source navigation payloads.

  Done means Prism diagnostics include enough location data for editor to open:



* [ ] Keep UI outside Prism.

  Done means Prism emits reports; SagaEditor decides how to display them.

---

## 20. Saga Product Integration

* [ ] Support Saga dashboard/build/publish status.

  Done means Saga can consume Prism summary reports to show:

  * stale artifact count,
  * boundary violation count,
  * package mismatch count,
  * generated output mismatch count,
  * publish-blocking Prism diagnostics.

* [ ] Keep product lifecycle outside Prism.

  Done means Prism does not own project dashboard, mode switching, or product workflow.

---

## 21. Configuration

* [ ] Define Prism configuration file.

  Possible file:

  ```txt
  prism.toml
  ```

  or integrated Forge/Saga project sections where appropriate.

* [ ] Support rule sets.

  Done means configuration can declare:

  * source roots,
  * generated roots,
  * manifest roots,
  * exclude rules,
  * boundary rules,
  * stale check policies,
  * report output path,
  * profile-specific severities.

* [ ] Keep config optional where defaults are clear.

  Done means Saga projects can use conventional paths without excessive Prism config.

---

## 22. Cache and Incrementality

* [ ] Add analysis cache.

  Done means Prism can avoid re-indexing unchanged files using:

  * file path,
  * content hash,
  * configuration hash,
  * Prism version,
  * parser/indexer version.

* [ ] Reject stale analysis cache.

  Done means cache is invalidated when:

  * source changes,
  * config changes,
  * Prism version changes,
  * manifest changes,
  * compile database changes.

* [ ] Keep cache transparent.

  Done means `--explain` or report metadata can show cache hits/misses for major analysis categories.

---

## 23. Security and Safety

* [ ] Treat project files as untrusted input.

  Done means Prism handles:

  * malformed manifests,
  * huge files,
  * cyclic references,
  * invalid encodings,
  * missing files,
  * broken symlinks,
  * path traversal attempts.

* [ ] Avoid destructive operations by default.

  Done means Prism does not modify source/generated/artifact files unless an explicitly safe formatting/fix command is later designed and documented.

* [ ] Keep read-only analysis default.

  Prism's default posture should be:

  ```txt
  read, analyze, report
  ```

  not:

  ```txt
  mutate, repair, hope
  ```

---

## 24. Testing Strategy

### 24.1 Source Index Tests

* [ ] Add source discovery/index tests.

  Required coverage:



---

### 24.2 C++ Dependency Tests

* [ ] Add C++ include/dependency tests.

  Required coverage:

  * direct include edge,
  * transitive include path,
  * missing include diagnostic,
  * forbidden module dependency,
  * public header Qt include violation,
  * Engine → Editor violation.

---





  Required coverage:

  * artifact manifest read,
  * source map read,
  * dependency manifest read,
  * stale source hash detection,
  * missing artifact detection,
  * hash mismatch detection.

---

### 24.4 Graph/Generated Code Tests

* [ ] Add graph/generated relationship tests.

  Required coverage:

  * graph source to generated C# mapping,
  * node-to-generated-region mapping,
  * stale generated C# detection,
  * missing source map detection,
  * graph artifact package mismatch.

---

### 24.5 Script Binding Tests

* [ ] Add script binding analysis tests.

  Required coverage:

  * block-callable method indexed,
  * binding manifest read,
  * signature hash mismatch,
  * authority metadata mismatch,
  * missing binding manifest entry,
  * stale generated binding metadata.

---

### 24.6 Asset Artifact Tests

* [ ] Add asset/cook analysis tests.

  Required coverage:

  * source asset to cooked artifact mapping,
  * import settings hash mismatch,
  * source hash changed,
  * missing cooked artifact,
  * package includes stale asset,
  * broken asset reference.

---

### 24.7 Package Tests

* [ ] Add package manifest analysis tests.

  Required coverage:

  * valid client package,
  * valid server package,
  * missing artifact,
  * listed artifact absent,
  * unlisted artifact present,
  * package kind mismatch,
  * server-only artifact in client package diagnostic.

---

### 24.8 Report Tests

* [ ] Add report output tests.

  Required coverage:

  * deterministic report ordering,
  * JSON schema validity,
  * diagnostics contain source/resource refs,
  * editor navigation payload present,
  * Forge-consumable summary present.

---

## 25. MVP Vertical Slice

The first Prism production slice should analyze a minimal Saga project produced by the Forge MVP.

Required project contents:



Required workflow:

```txt
prism index --json
prism boundaries --profile ci --json
prism stale --profile dev-local-client-server --json
prism packages --profile dev-local-client-server --json
```

Required behavior:



This slice proves Prism is not a fancy grep.

It is artifact relationship intelligence.

---

## 26. Non-Goals

Prism must not become:



Prism finds evidence.

Other systems decide how to act on that evidence.

---

## 27. Risk Register

### 27.1 Risk: Prism Becomes Build System

Mitigation:

* Prism emits reports only,
* Forge owns build/publish decisions,
* no artifact generation in Prism,
* no package staging in Prism.

---



Mitigation:

* read manifests/reports/public formats,
* forbid private includes,
* add boundary tests,
* keep report format stable.

---

### 27.3 Risk: Stale Detection Is Incomplete

Mitigation:

* require source hashes,
* require generated origin manifests,
* require dependency manifests,
* compare tool/schema/profile versions,
* report unknown freshness as warning/error by profile.

---

### 27.4 Risk: Reports Are Too Abstract for Editor Navigation

Mitigation:

* include source/resource/artifact refs,
* include source ranges where available,
* include graph node/pin/edge refs where available,
* keep raw payload available for technical profiles.

---

### 27.5 Risk: Prism Becomes Too Saga-Specific

Mitigation:

* keep generic source/dependency/index features,
* make Saga-specific checks profile/config driven,
* support standalone C++/generated-output use cases.

---

## 28. Suggested File Targets

Expected core files:

```txt
Tools/Prism/include/Prism/Core/PrismConfig.hpp
Tools/Prism/include/Prism/Core/PrismContext.hpp
Tools/Prism/include/Prism/Core/AnalysisProfile.hpp
Tools/Prism/include/Prism/Core/AnalysisResult.hpp
Tools/Prism/src/Core/PrismContext.cpp
```

Expected discovery/index files:

```txt
Tools/Prism/include/Prism/Index/SourceDiscovery.hpp
Tools/Prism/include/Prism/Index/FileHashIndex.hpp
Tools/Prism/include/Prism/Index/ProjectIndex.hpp
Tools/Prism/src/Index/SourceDiscovery.cpp
Tools/Prism/src/Index/FileHashIndex.cpp
```

Expected C++ files:

```txt
Tools/Prism/include/Prism/Cpp/CppIncludeIndexer.hpp
Tools/Prism/include/Prism/Cpp/CppIncludeGraph.hpp
Tools/Prism/include/Prism/Cpp/CompileDatabaseReader.hpp
Tools/Prism/src/Cpp/CppIncludeIndexer.cpp
Tools/Prism/src/Cpp/CompileDatabaseReader.cpp
```

Expected scripting files:

```txt
Tools/Prism/include/Prism/Scripting/CSharpSourceIndexer.hpp
Tools/Prism/include/Prism/Scripting/BlockCallableIndex.hpp
Tools/Prism/include/Prism/Scripting/ScriptBindingManifestReader.hpp
Tools/Prism/src/Scripting/CSharpSourceIndexer.cpp
Tools/Prism/src/Scripting/BlockCallableIndex.cpp
```





Expected asset/package files:

```txt
Tools/Prism/include/Prism/Assets/AssetArtifactIndex.hpp
Tools/Prism/include/Prism/Packages/PackageManifestIndex.hpp
Tools/Prism/src/Assets/AssetArtifactIndex.cpp
Tools/Prism/src/Packages/PackageManifestIndex.cpp
```

Expected validation files:

```txt
Tools/Prism/include/Prism/Validation/BoundaryValidator.hpp
Tools/Prism/include/Prism/Validation/StaleGraphArtifactCheck.hpp
Tools/Prism/include/Prism/Validation/StaleScriptArtifactCheck.hpp
Tools/Prism/include/Prism/Validation/StaleCookedArtifactCheck.hpp
Tools/Prism/include/Prism/Validation/PackageManifestConsistencyCheck.hpp
Tools/Prism/include/Prism/Validation/AuthorityMetadataConsistencyCheck.hpp
Tools/Prism/src/Validation/BoundaryValidator.cpp
Tools/Prism/src/Validation/StaleGraphArtifactCheck.cpp
Tools/Prism/src/Validation/StaleScriptArtifactCheck.cpp
Tools/Prism/src/Validation/StaleCookedArtifactCheck.cpp
```

Expected report files:

```txt
Tools/Prism/include/Prism/Reports/PrismReport.hpp
Tools/Prism/include/Prism/Reports/ReportWriter.hpp
Tools/Prism/include/Prism/Diagnostics/PrismDiagnostic.hpp
Tools/Prism/src/Reports/ReportWriter.cpp
```

---

## 29. Decision Summary

Preserve these decisions:



Prism should make hidden relationships visible.

If it starts generating the relationships it is supposed to verify, it has crossed the line and become another build system.
