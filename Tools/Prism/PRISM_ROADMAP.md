# Prism — Code Intelligence Roadmap

> Last updated: 2026-05-14  
> Status: Active roadmap  
> Target: A production-grade code intelligence and graph analysis tool for Saga projects.  
> Scope: Source indexing, symbol discovery, include/dependency graph generation, semantic metadata extraction, query APIs, diagnostics, stale generated-code detection, build insight support, and integration with Forge, SDE, SagaTools, and editor tooling through stable outputs.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files, modules, or integration points that represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item describes the finished production state rather than interim scaffolding.
- Shipped items must name the files, modules, or integration points that represent the completed work.
- Open items must describe the finished state, not temporary scaffolding.
- Prism owns code intelligence and graph analysis.
- Prism does not own SDE compiler truth.
- Prism does not own Forge build workflow.
- Prism does not own SagaTools dispatch.
- Prism does not own Saga product shell.
- Prism does not own editor UI.
- Prism may consume stable outputs from SDE, Forge, and shared contracts where explicitly approved.

---

## 1. Document Purpose

This document defines the roadmap for Prism.

Prism is Saga’s code intelligence and graph analysis tool.

It exists to help developers understand the codebase, generated artifacts, symbol relationships, dependency direction, include graphs, and stale or invalid generated outputs.

Prism owns:

- source indexing,
- symbol discovery,
- include graph analysis,
- dependency graph generation,
- code ownership graph generation,
- generated-code relationship analysis,
- stale generated-code detection,
- semantic metadata extraction where safe,
- query APIs,
- diagnostics,
- graph export,
- build insight support.

Prism does not own:

- SDE parsing truth,
- SDE semantic validation truth,
- SDE artifact emission,
- Forge build workflow execution,
- SagaTools command routing,
- Saga product shell,
- editor UI,
- runtime/server execution,
- collaboration implementation.

Correct model:

```txt
Prism
  indexes and analyzes code/artifact relationships

SDE
  compiles deterministic data definitions

Forge
  coordinates build workflow

SagaTools
  dispatches tool commands

Editor/Product
  may consume Prism outputs through stable reports or APIs
```

Incorrect model:

```txt
Prism becomes compiler, build tool, editor backend, and product service.
```

That is not code intelligence.

That is a tool having a midlife crisis with a graph database.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `docs/roadmaps/TOOLS_ROADMAP.md` | Tool ecosystem ownership index |
| `Tools/Prism/PRISM_ROADMAP.md` | Prism code intelligence roadmap |
| `Tools/SystemDefinitionEngine/SDE_ROADMAP.md` | SDE deterministic data compiler roadmap |
| `Tools/Forge/FORGE_ROADMAP.md` | Forge build workflow frontend roadmap |
| `Tools/SagaTools/SAGATOOLS_ROADMAP.md` | Thin tool dispatcher roadmap |
| `SHARED_ROADMAP.md` | Shared contracts and artifact references |
| `DependencyGraph.md` | Dependency ownership and compile-time architecture rules |
| `ENGINE_ROADMAP.md` | Runtime/server ownership boundaries |
| `EDITOR_ROADMAP.md` | Editor-side consumption boundaries |

---

## 3. Ownership Boundary

- [x] Define Prism as code intelligence and graph builder.

  Represented by:

  ```txt
  Tools/Prism/PRISM_ROADMAP.md
  docs/roadmaps/TOOLS_ROADMAP.md
  DependencyGraph.md
  ```

  Preserved decision:

  ```txt
  Prism indexes and analyzes.
  Prism does not compile, build, dispatch, run the editor, or own product workflow.
  ```

- [ ] Keep Prism focused on code intelligence.

  Done means Prism owns:

  - source file discovery,
  - symbol extraction,
  - include graph generation,
  - dependency graph analysis,
  - ownership boundary reports,
  - generated artifact relationship reports,
  - stale generated-code detection,
  - query output,
  - graph export,
  - diagnostics.

- [ ] Prevent Prism from owning other tool internals.

  Done means Prism does not own:

  - SDE parser,
  - SDE AST,
  - SDE semantic analyzer,
  - SDE artifact emitter,
  - Forge build planner,
  - Forge package builder,
  - SagaTools command dispatcher,
  - editor panels,
  - runtime/server implementation.

---

## 4. Dependency Rules

### 4.1 Allowed Dependencies

- [ ] Allow Prism to consume source files and compile metadata.

  Allowed inputs:

  ```txt
  C++ source files
  C++ headers
  generated source files
  CMake compile commands
  package manifests
  artifact manifests
  SDE output manifests
  Forge build reports
  shared contract metadata where explicitly approved
  ```

- [ ] Allow Prism to consume SDE outputs.

  Allowed SDE-derived inputs:

  ```txt
  artifact manifest
  source map
  symbol metadata
  schema metadata
  generated code manifest
  diagnostic output
  ```

- [ ] Allow Prism to consume Forge outputs.

  Allowed Forge-derived inputs:

  ```txt
  build report
  artifact manifest
  build graph report
  staged artifact manifest
  diagnostics report
  ```

- [ ] Allow SagaTools to dispatch Prism.

  Correct direction:

  ```txt
  SagaTools → Prism executable
  ```

---

### 4.2 Forbidden Dependencies

- [ ] Prevent Prism from depending on SDE internals.

  Forbidden:

  ```txt
  Prism → SDE parser internals
  Prism → SDE AST internals
  Prism → SDE semantic analyzer
  Prism → SDE IR internals
  Prism → SDE codegen internals
  ```

- [ ] Prevent Prism from depending on Forge internals.

  Forbidden:

  ```txt
  Prism → Forge build planner internals
  Prism → Forge cache internals
  Prism → Forge package builder internals
  Prism → Forge asset cook planner internals
  ```

- [ ] Prevent Prism from depending on editor/runtime/server private implementation.

  Forbidden:

  ```txt
  Prism → SagaEditor UI
  Prism → SagaEngine runtime private internals
  Prism → SagaServer private headers
  Prism → Saga product shell internals
  Prism → SagaCollaboration implementation internals
  ```

- [ ] Prevent Prism from becoming global tool dispatcher.

  Forbidden:

  ```txt
  Prism owns sagatools command routing
  Prism dispatches SDE/Forge as primary workflow
  Prism replaces SagaTools list/doctor behavior
  ```

Prism observes and analyzes.

It does not become mayor of the toolchain.

---

## 5. CLI Roadmap

- [ ] Provide stable Prism CLI.

  Required commands:

  ```txt
  prism help
  prism version
  prism doctor
  prism index
  prism query
  prism graph
  prism inspect
  prism validate-boundaries
  prism stale
  prism export
  ```

- [ ] Provide `prism index`.

  Done means:

  - workspace is resolved,
  - source roots are discovered,
  - compile commands are loaded where available,
  - files are indexed,
  - symbols are extracted,
  - include graph is generated,
  - diagnostics are emitted,
  - index database is written or refreshed.

- [ ] Provide `prism query`.

  Done means users can query:

  - symbol by name,
  - symbol references,
  - file dependencies,
  - include relationships,
  - module ownership,
  - generated-code origin,
  - artifact relationships.

- [ ] Provide `prism graph`.

  Done means Prism can generate:

  - include graph,
  - module dependency graph,
  - ownership graph,
  - generated artifact graph,
  - SDE output relationship graph,
  - build insight graph where Forge output is available.

- [ ] Provide `prism validate-boundaries`.

  Done means Prism can report architecture boundary violations such as:

  ```txt
  Engine includes Editor
  Server includes Editor collaboration headers
  SagaShared includes implementation headers
  SDE depends on Saga modules
  Tools import each other's internals
  ```

- [ ] Provide `prism stale`.

  Done means Prism can detect stale generated code or artifacts when source and generated outputs do not match expected hashes or manifests.

---

## 6. Workspace and Project Discovery

- [ ] Add workspace discovery.

  Done means Prism can resolve:

  - explicit workspace path,
  - current directory workspace,
  - project manifest,
  - compile commands path,
  - source roots,
  - generated output roots,
  - artifact manifest locations.

Expected files:

```txt
Tools/Prism/include/Prism/Workspace/WorkspaceContext.hpp
Tools/Prism/include/Prism/Workspace/WorkspaceLocator.hpp
Tools/Prism/src/Workspace/WorkspaceLocator.cpp
```

- [ ] Add workspace validation.

  Done means Prism validates:

  - workspace root exists,
  - source roots exist,
  - compile commands are readable where required,
  - generated directories are readable where configured,
  - output/index directory is writable,
  - configured manifests are valid.

- [ ] Keep product project lifecycle outside Prism.

  Done means Prism does not own:

  - Saga project dashboard,
  - project creation,
  - project opening,
  - recent project registry,
  - product mode switching.

Prism can analyze a workspace.

It does not become the product shell.

A map is useful. A map that tries to drive the car is a lawsuit.

---

## 7. Source Indexing

- [ ] Add source file discovery.

  Done means Prism can discover:

  - headers,
  - source files,
  - generated files,
  - tool-owned files,
  - excluded directories,
  - third-party boundaries.

Expected files:

```txt
Tools/Prism/include/Prism/Index/SourceFile.hpp
Tools/Prism/include/Prism/Index/SourceFileScanner.hpp
Tools/Prism/src/Index/SourceFileScanner.cpp
```

- [ ] Add compile command support.

  Done means Prism can read:

  ```txt
  compile_commands.json
  ```

  and use it for accurate parsing/indexing where available.

- [ ] Add indexing database.

  Done means Prism stores:

  - file ids,
  - file paths,
  - content hashes,
  - last indexed time or deterministic index version,
  - symbols,
  - references,
  - include edges,
  - diagnostics.

Expected files:

```txt
Tools/Prism/include/Prism/Index/IndexDatabase.hpp
Tools/Prism/include/Prism/Index/IndexRecord.hpp
Tools/Prism/src/Index/IndexDatabase.cpp
```

- [ ] Add incremental indexing.

  Done means unchanged files are skipped safely based on:

  - content hash,
  - compile command hash,
  - index schema version,
  - tool version,
  - dependency state.

---

## 8. Symbol Discovery

- [ ] Add symbol extraction.

  Done means Prism can discover:

  - namespaces,
  - classes,
  - structs,
  - enums,
  - functions,
  - methods,
  - fields,
  - typedefs/type aliases,
  - constants,
  - macros where supported.

Expected files:

```txt
Tools/Prism/include/Prism/Symbols/Symbol.hpp
Tools/Prism/include/Prism/Symbols/SymbolKind.hpp
Tools/Prism/include/Prism/Symbols/SymbolExtractor.hpp
Tools/Prism/src/Symbols/SymbolExtractor.cpp
```

- [ ] Add symbol reference tracking.

  Done means Prism can answer:

  - where symbol is defined,
  - where symbol is declared,
  - where symbol is referenced,
  - which files depend on symbol,
  - which module owns symbol.

- [ ] Add symbol query output.

  Done means `prism query <symbol>` can show:

  - symbol kind,
  - definition location,
  - declaration locations,
  - references,
  - owning module,
  - generated/source origin,
  - related diagnostics.

---

## 9. Include Graph

- [ ] Add include graph generation.

  Done means Prism can build a graph of:

  - file includes,
  - direct include edges,
  - transitive include edges,
  - missing includes,
  - cyclic includes,
  - private/public include boundary violations.

Expected files:

```txt
Tools/Prism/include/Prism/Graph/IncludeGraph.hpp
Tools/Prism/include/Prism/Graph/IncludeEdge.hpp
Tools/Prism/src/Graph/IncludeGraph.cpp
```

- [ ] Add include cycle detection.

  Done means Prism can report:

  - cycle path,
  - involved files,
  - owning modules,
  - severity,
  - suggested boundary fix where possible.

- [ ] Add include weight diagnostics.

  Done means Prism can report:

  - heavily included headers,
  - expensive include fanout,
  - unnecessary public includes where detectable,
  - private headers included across module boundary.

Include graphs exist because apparently `#include` looked too innocent to be dangerous.

It was not.

---

## 10. Module Dependency Graph

- [ ] Add module ownership model.

  Done means Prism can classify files into modules such as:

  ```txt
  Saga
  SagaEditor
  SagaEngine
  SagaServer
  SagaShared
  SagaCollaboration
  Tools/SDE
  Tools/Forge
  Tools/Prism
  Tools/SagaTools
  ThirdParty
  ```

Expected files:

```txt
Tools/Prism/include/Prism/Modules/ModuleId.hpp
Tools/Prism/include/Prism/Modules/ModuleRule.hpp
Tools/Prism/include/Prism/Modules/ModuleClassifier.hpp
Tools/Prism/src/Modules/ModuleClassifier.cpp
```

- [ ] Add module dependency graph.

  Done means Prism can generate:

  - module nodes,
  - include edges,
  - link dependency edges where available,
  - generated artifact edges,
  - forbidden dependency edges,
  - dependency summaries.

Expected files:

```txt
Tools/Prism/include/Prism/Graph/ModuleDependencyGraph.hpp
Tools/Prism/include/Prism/Graph/ModuleEdge.hpp
Tools/Prism/src/Graph/ModuleDependencyGraph.cpp
```

- [ ] Add forbidden dependency detection.

  Done means Prism detects violations such as:

  ```txt
  Engine/Core → Editor
  Engine/Core → Apps/Saga
  Server → Editor
  Runtime → Editor/include/SagaEditor/Collaboration
  SagaShared → SagaCollaboration
  SDE → SagaEngine
  Forge → SDE internals
  SagaTools → Forge internals
  ```

---

## 11. Architecture Boundary Validation

- [ ] Encode dependency rules from `DependencyGraph.md`.

  Done means Prism can validate rules from:

  ```txt
  DependencyGraph.md
  ```

  or from a structured equivalent config.

- [ ] Add boundary validation report.

  Done means report includes:

  - violated rule,
  - source file,
  - included/imported target,
  - source module,
  - target module,
  - severity,
  - suggested owner boundary.

Expected output:

```txt
Boundary violation:
  Rule: Engine/Core must not include Editor
  Source: Engine/src/...
  Target: Editor/include/...
  Suggested fix: move neutral contract to SagaShared or add runtime-facing API.
```

- [ ] Support machine-readable boundary report.

  Done means CI can consume boundary violations as JSON.

- [ ] Support CI failure mode.

  Done means `prism validate-boundaries --fail-on-error` exits non-zero on forbidden dependency violations.

---

## 12. Generated Code and Artifact Relationships

- [ ] Track generated code origin.

  Done means Prism can identify:

  - generated file,
  - generator tool,
  - source schema/resource,
  - generation manifest,
  - artifact id,
  - content hash,
  - generation timestamp or deterministic build id.

Expected files:

```txt
Tools/Prism/include/Prism/Generated/GeneratedFile.hpp
Tools/Prism/include/Prism/Generated/GeneratedOrigin.hpp
Tools/Prism/src/Generated/GeneratedTracker.cpp
```

- [ ] Detect stale generated code.

  Done means Prism can compare:

  - source hash,
  - manifest hash,
  - generated file hash,
  - expected artifact hash,
  - generator version,
  - schema version.

- [ ] Explain stale generated-code diagnostics.

  Done means stale report includes:

  - generated file,
  - source file,
  - expected hash,
  - actual hash,
  - generator tool,
  - suggested regeneration command.

A stale generated file is not “probably fine”.

It is a lie with a `.cpp` extension.

---

## 13. SDE Output Analysis

- [ ] Consume SDE artifact manifests.

  Done means Prism can read SDE outputs such as:

  - artifact manifest,
  - schema ids,
  - generated code references,
  - source maps,
  - diagnostics,
  - dependency graph.

- [ ] Link SDE source to generated outputs.

  Done means Prism can answer:

  - which SDE source generated this file,
  - which runtime artifact came from this schema,
  - which code symbols were generated from SDE definitions,
  - which build artifacts depend on SDE output.

- [ ] Preserve SDE ownership.

  Done means Prism does not include:

  ```txt
  SDE parser internals
  SDE AST internals
  SDE semantic analyzer
  SDE IR internals
  SDE codegen internals
  ```

Prism reads SDE outputs.

SDE remains compiler truth.

Nobody needs a second secret compiler hiding in the code intelligence tool.

---

## 14. Forge Output Analysis

- [ ] Consume Forge build reports.

  Done means Prism can read:

  - build plan report,
  - build artifact manifest,
  - diagnostics report,
  - build profile,
  - staged artifact list,
  - cache hit/miss data where exposed.

- [ ] Link build steps to source/artifact graph.

  Done means Prism can answer:

  - which build step produced this artifact,
  - which source files contributed to this output,
  - which generated files are stale relative to build report,
  - which module depends on which artifact.

- [ ] Preserve Forge ownership.

  Done means Prism does not include Forge build planner or cache internals.

Forge builds.

Prism observes the build graph.

This division of labor is not decorative.

---

## 15. Query System

- [ ] Add query model.

  Done means Prism supports queries for:

  - symbols,
  - files,
  - modules,
  - includes,
  - dependencies,
  - generated files,
  - artifacts,
  - diagnostics.

Expected files:

```txt
Tools/Prism/include/Prism/Query/Query.hpp
Tools/Prism/include/Prism/Query/QueryResult.hpp
Tools/Prism/include/Prism/Query/QueryEngine.hpp
Tools/Prism/src/Query/QueryEngine.cpp
```

- [ ] Add common queries.

  Required queries:

  ```txt
  symbol <name>
  refs <symbol>
  file <path>
  includes <path>
  depends-on <module>
  depended-by <module>
  generated-from <file>
  stale
  boundary-violations
  ```

- [ ] Add JSON query output.

  Done means query output is stable enough for editor/CI/tool consumption.

- [ ] Add human-readable query output.

  Done means CLI output is useful without requiring the user to mentally parse a graph dump like a condemned librarian.

---

## 16. Graph Export

- [ ] Add graph export support.

  Required formats:

  ```txt
  json
  dot
  graphml optional
  mermaid optional
  ```

- [ ] Add include graph export.

- [ ] Add module dependency graph export.

- [ ] Add generated artifact graph export.

- [ ] Add boundary violation graph export.

- [ ] Keep graph exports deterministic.

  Done means identical inputs produce stable node/edge ordering.

---

## 17. Diagnostics

- [ ] Add Prism diagnostic model.

  Done means diagnostics include:

  - severity,
  - code,
  - message,
  - source file,
  - source range where available,
  - module,
  - related file/module,
  - recoverability,
  - suggested action.

Expected files:

```txt
Tools/Prism/include/Prism/Diagnostics/PrismDiagnostic.hpp
Tools/Prism/include/Prism/Diagnostics/DiagnosticCode.hpp
Tools/Prism/include/Prism/Diagnostics/DiagnosticSeverity.hpp
Tools/Prism/src/Diagnostics/PrismDiagnostic.cpp
```

- [ ] Add diagnostics for indexing failures.

  Required diagnostics:

  - unreadable file,
  - parse failure,
  - compile command missing,
  - unsupported language mode,
  - missing include,
  - index database write failure.

- [ ] Add diagnostics for graph issues.

  Required diagnostics:

  - include cycle,
  - forbidden dependency,
  - missing generated origin,
  - stale generated file,
  - module classification failure.

- [ ] Support JSON diagnostics.

  Done means editor, Forge, SagaTools, and CI can consume Prism diagnostics.

---

## 18. Index Storage

- [ ] Add persistent index database.

  Done means Prism can persist:

  - file records,
  - symbol records,
  - reference records,
  - include edges,
  - module edges,
  - generated file records,
  - artifact relationships,
  - diagnostics.

Expected files:

```txt
Tools/Prism/include/Prism/Storage/IndexStore.hpp
Tools/Prism/include/Prism/Storage/IndexSchema.hpp
Tools/Prism/src/Storage/IndexStore.cpp
```

- [ ] Add index schema versioning.

  Done means incompatible index schema changes trigger rebuild.

- [ ] Add index corruption recovery.

  Done means corrupted index is detected and rebuilt rather than producing nonsense results with a straight face.

- [ ] Add index compaction/cleanup.

  Done means removed files and stale records are cleaned safely.

---

## 19. Incremental Analysis

- [ ] Add incremental source analysis.

  Done means Prism re-indexes only files affected by:

  - file content hash changes,
  - compile command changes,
  - dependency changes,
  - index schema changes,
  - tool version changes.

- [ ] Add dependency-based invalidation.

  Done means changes to a header can invalidate dependent files where needed.

- [ ] Add generated artifact invalidation.

  Done means changes to SDE/Forge manifests can invalidate generated-code relationship records.

---

## 20. Performance

- [ ] Add indexing performance metrics.

  Done means Prism records:

  - files scanned,
  - files indexed,
  - files skipped,
  - symbols discovered,
  - include edges discovered,
  - graph build time,
  - database write time,
  - total index time.

- [ ] Add large workspace performance target.

  Done means Prism can index a representative large Saga workspace within acceptable time and memory bounds.

- [ ] Add memory usage bounds.

  Done means graph construction and indexing avoid unbounded memory growth.

- [ ] Add slow file diagnostics.

  Done means expensive files or graph steps can be identified.

---

## 21. Editor Integration

- [ ] Allow editor to consume Prism reports through stable outputs.

  Done means SagaEditor can display:

  - symbol search results,
  - dependency graph results,
  - boundary violations,
  - stale generated-code diagnostics,
  - include graph warnings.

- [ ] Keep editor UI outside Prism.

  Done means Prism does not own:

  - editor panels,
  - graph visualization widgets,
  - search UI,
  - Problems panel,
  - project dashboard.

Correct flow:

```txt
Prism indexes/analyzes
      ↓
Prism emits report/query result
      ↓
Editor displays report
```

Incorrect flow:

```txt
Prism imports editor widgets and updates panels directly
```

No.

Bad tool.

---

## 22. Forge Integration

- [ ] Allow Forge to optionally invoke Prism for build insight.

  Done means Forge can use Prism for:

  - dependency insight,
  - stale generated-code detection,
  - boundary validation before build,
  - source graph diagnostics.

- [ ] Keep Prism optional unless build profile requires it.

  Done means normal build does not fail because Prism is unavailable unless selected profile explicitly requires Prism.

- [ ] Keep Forge as build workflow owner.

  Done means Prism does not execute Forge build plans.

---

## 23. SagaTools Integration

- [ ] Allow SagaTools to dispatch Prism.

  Example commands:

  ```txt
  sagatools prism index
  sagatools prism query <symbol>
  sagatools prism graph
  sagatools prism doctor
  ```

- [ ] Keep SagaTools as dispatcher only.

  Done means SagaTools does not parse Prism index internals or graph database.

- [ ] Keep Prism usable standalone.

  Done means Prism can still run as:

  ```txt
  prism index
  prism query <symbol>
  ```

  without requiring SagaTools.

---

## 24. CI Integration

- [ ] Add boundary validation command for CI.

  Required command:

  ```txt
  prism validate-boundaries --workspace <path> --fail-on-error
  ```

- [ ] Add stale generated-code check for CI.

  Required command:

  ```txt
  prism stale --workspace <path> --fail-on-stale
  ```

- [ ] Add graph export for CI artifacts.

  Done means CI can upload:

  ```txt
  include-graph.json
  module-graph.json
  boundary-violations.json
  stale-generated.json
  ```

- [ ] Add regression checks for forbidden dependencies.

  Required checks:

  ```txt
  Engine/Core must not include Editor
  Server must not include Editor
  Runtime/Server must not include Editor/include/SagaEditor/Collaboration
  SagaShared must not include SagaCollaboration
  SDE must not include Saga modules
  Forge must not include SDE internals
  SagaTools must not include tool internals
  ```

---

## 25. Configuration

- [ ] Add Prism configuration support.

  Done means config can define:

  - workspace roots,
  - source roots,
  - generated roots,
  - excluded paths,
  - compile commands path,
  - module classification rules,
  - dependency boundary rules,
  - output/report paths,
  - index database path.

Expected files:

```txt
Tools/Prism/include/Prism/Config/PrismConfig.hpp
Tools/Prism/include/Prism/Config/PrismConfigLoader.hpp
Tools/Prism/src/Config/PrismConfigLoader.cpp
```

- [ ] Add command-line config overrides.

  Done means CLI can override:

  - workspace path,
  - index path,
  - output format,
  - graph type,
  - fail-on-error behavior,
  - verbosity.

- [ ] Add config validation.

  Done means invalid config fails before indexing begins.

---

## 26. Doctor Command

- [ ] Add `prism doctor`.

  Done means doctor checks:

  - workspace detection,
  - source roots,
  - compile commands path,
  - index database path,
  - config validity,
  - graph export path,
  - optional SDE manifest availability,
  - optional Forge report availability.

- [ ] Keep doctor focused on Prism health.

  Done means `prism doctor` does not deeply validate:

  - SDE compiler correctness,
  - Forge build correctness,
  - SagaTools dispatcher behavior,
  - editor runtime behavior.

Each tool owns its own existential problems.

Prism has enough.

---

## 27. Exit Codes

- [ ] Define stable Prism exit codes.

  Required categories:

  ```txt
  0   success
  1   general failure
  2   invalid arguments
  3   workspace error
  4   config error
  5   indexing failure
  6   query failure
  7   graph generation failure
  8   boundary violation
  9   stale generated output
  10  index database error
  11  internal error
  ```

- [ ] Keep boundary/stale failures distinguishable.

  Done means CI can tell whether failure came from:

  - invalid command,
  - indexing error,
  - architecture violation,
  - stale generated code,
  - internal Prism bug.

---

## 28. Logging

- [ ] Add Prism logging.

  Done means logs include:

  - selected command,
  - workspace path,
  - indexed files count,
  - skipped files count,
  - symbol count,
  - graph edge count,
  - diagnostics count,
  - elapsed time.

- [ ] Keep normal output readable.

  Done means default CLI output is not an unfiltered landfill of graph facts.

- [ ] Add verbose mode.

  Done means `--verbose` shows detailed indexing and graph construction steps.

---

## 29. Testing Roadmap

### 29.1 Unit Tests

- [ ] Add workspace locator tests.

- [ ] Add config loader tests.

- [ ] Add source scanner tests.

- [ ] Add symbol extraction tests.

- [ ] Add include graph tests.

- [ ] Add module classifier tests.

- [ ] Add boundary rule tests.

- [ ] Add generated origin tracker tests.

- [ ] Add query engine tests.

- [ ] Add graph export tests.

---

### 29.2 Integration Tests

- [ ] Add indexing integration test.

  Done means Prism can index a small test workspace and produce expected symbols/includes.

- [ ] Add boundary validation integration test.

  Done means test workspace with forbidden include produces expected diagnostic.

- [ ] Add stale generated-code integration test.

  Done means stale generated output is detected from manifest/hash mismatch.

- [ ] Add SDE manifest integration test.

  Done means Prism can read fake or real SDE output manifest and link source to generated output.

- [ ] Add Forge report integration test.

  Done means Prism can read fake or real Forge build report and link build steps to artifacts.

---

### 29.3 Snapshot Tests

- [ ] Add graph JSON snapshot tests.

- [ ] Add diagnostic JSON snapshot tests.

- [ ] Add query output snapshot tests.

- [ ] Add boundary report snapshot tests.

Snapshot output must be stable.

Otherwise the test suite becomes a random number generator with moral superiority.

---

### 29.4 Performance Tests

- [ ] Add large workspace indexing benchmark.

- [ ] Add include graph build benchmark.

- [ ] Add query latency benchmark.

- [ ] Add incremental indexing benchmark.

---

## 30. CI Requirements

- [ ] Add Prism unit tests to CI.

- [ ] Add Prism integration tests to CI.

- [ ] Add Prism CLI smoke tests.

  Required commands:

  ```txt
  prism --help
  prism version
  prism doctor
  prism index --workspace <test-workspace>
  prism validate-boundaries --workspace <test-workspace>
  ```

- [ ] Add dependency boundary checks for Prism itself.

  Required forbidden checks:

  ```txt
  Tools/Prism/** must not include Tools/SystemDefinitionEngine/src/**
  Tools/Prism/** must not include Tools/Forge/src/**
  Tools/Prism/** must not include Tools/SagaTools/src/**
  Tools/Prism/** must not include Editor/**
  Tools/Prism/** must not include Server/private/**
  Tools/Prism/** must not include Apps/Saga/**
  ```

- [ ] Add JSON output compatibility test.

  Done means Prism JSON output remains parseable and schema-stable.

---

## 31. Recommended File Layout

Recommended target layout:

```txt
Tools/Prism/
  PRISM_ROADMAP.md
  README.md
  CMakeLists.txt or Cargo.toml

Tools/Prism/docs/
  PRISM_CLI.md
  PRISM_QUERY_LANGUAGE.md
  PRISM_GRAPH_FORMATS.md
  PRISM_BOUNDARY_RULES.md
  PRISM_DIAGNOSTICS.md

Tools/Prism/include/Prism/
  Prism.hpp
  PrismCommand.hpp
  PrismResult.hpp

Tools/Prism/include/Prism/Workspace/
  WorkspaceContext.hpp
  WorkspaceLocator.hpp

Tools/Prism/include/Prism/Config/
  PrismConfig.hpp
  PrismConfigLoader.hpp

Tools/Prism/include/Prism/Index/
  SourceFile.hpp
  SourceFileScanner.hpp
  IndexDatabase.hpp
  IndexRecord.hpp

Tools/Prism/include/Prism/Symbols/
  Symbol.hpp
  SymbolKind.hpp
  SymbolExtractor.hpp

Tools/Prism/include/Prism/Graph/
  IncludeGraph.hpp
  IncludeEdge.hpp
  ModuleDependencyGraph.hpp
  ModuleEdge.hpp
  GraphExporter.hpp

Tools/Prism/include/Prism/Modules/
  ModuleId.hpp
  ModuleRule.hpp
  ModuleClassifier.hpp

Tools/Prism/include/Prism/Generated/
  GeneratedFile.hpp
  GeneratedOrigin.hpp
  GeneratedTracker.hpp

Tools/Prism/include/Prism/Query/
  Query.hpp
  QueryResult.hpp
  QueryEngine.hpp

Tools/Prism/include/Prism/Diagnostics/
  PrismDiagnostic.hpp
  DiagnosticCode.hpp
  DiagnosticSeverity.hpp

Tools/Prism/include/Prism/Storage/
  IndexStore.hpp
  IndexSchema.hpp

Tools/Prism/src/
  main.cpp or main.rs
  Workspace/
  Config/
  Index/
  Symbols/
  Graph/
  Modules/
  Generated/
  Query/
  Diagnostics/
  Storage/

Tools/Prism/tests/
  WorkspaceLocatorTests.cpp
  SourceScannerTests.cpp
  SymbolExtractorTests.cpp
  IncludeGraphTests.cpp
  ModuleClassifierTests.cpp
  BoundaryRuleTests.cpp
  GeneratedTrackerTests.cpp
  QueryEngineTests.cpp
  IntegrationTests.cpp
```

This layout is illustrative.

The ownership rule is not.

---

## 32. Migration Plan

- [ ] Remove compiler ownership from Prism if present.

  Done means SDE internals live only in SDE.

- [ ] Remove build workflow ownership from Prism if present.

  Done means Forge owns build planning and package workflow.

- [ ] Remove tool dispatch ownership from Prism if present.

  Done means SagaTools owns top-level dispatch.

- [ ] Convert Prism to code intelligence and graph analysis only.

  Done means Prism owns:

  ```txt
  index
  query
  graph
  inspect
  stale detection
  boundary validation
  report/export
  ```

- [ ] Add stable input contracts for SDE/Forge outputs.

  Done means Prism reads output manifests/reports instead of importing private internals.

- [ ] Add dependency boundary checks.

---

## 33. Non-Goals

Prism does not own:

- SDE compiler implementation,
- Forge build workflow implementation,
- SagaTools command dispatch,
- Saga product shell,
- editor UI,
- runtime simulation,
- server authority,
- collaboration sessions,
- collaboration permissions,
- asset cooking,
- package building,
- source generation,
- generated code emission.

Related ownership:

| Area | Owner |
|---|---|
| Code intelligence and graph analysis | `Prism` |
| Deterministic data compiler | `SDE` |
| Build workflow frontend | `Forge` |
| Tool dispatch | `SagaTools` |
| Product shell | `Saga` |
| Authoring UI | `SagaEditor` |
| Runtime/server systems | `SagaEngine` / `SagaServer` |
| Collaboration implementation | `SagaCollaboration` |

---

## 34. Production Definition of Done

- [ ] Prism has a stable CLI.

- [ ] Prism can discover and validate workspaces.

- [ ] Prism can index source files.

- [ ] Prism can extract symbols and references.

- [ ] Prism can build include graphs.

- [ ] Prism can build module dependency graphs.

- [ ] Prism can validate dependency boundaries.

- [ ] Prism can detect stale generated code.

- [ ] Prism can consume SDE output manifests without depending on SDE internals.

- [ ] Prism can consume Forge reports without depending on Forge internals.

- [ ] Prism can answer common code intelligence queries.

- [ ] Prism can export graphs in stable formats.

- [ ] Prism emits human-readable and JSON diagnostics.

- [ ] Prism has stable exit codes.

- [ ] Prism integrates with SagaTools as a dispatched tool.

- [ ] Prism remains usable standalone.

- [ ] CI tests indexing, graph output, boundary validation, stale detection, and dependency rules.

---

## 35. Final Architecture Rule

Prism should remain:

```txt
a code intelligence tool,
a graph builder,
a boundary validator,
a generated-code relationship inspector,
and a reporting/query layer.
```

It should know:

```txt
where code lives,
which symbols exist,
which files include each other,
which modules depend on each other,
which generated files came from which sources,
which boundaries are violated,
and which outputs are stale.
```

It should not know:

```txt
how SDE parses and compiles schemas,
how Forge builds packages,
how SagaTools dispatches commands,
how Saga opens projects,
how the editor draws panels,
how runtime simulation works,
or how the server owns authority.
```

Prism succeeds when it makes the codebase easier to understand without becoming the codebase.

That is the whole trick.

Naturally, it is the trick most tools immediately fail.