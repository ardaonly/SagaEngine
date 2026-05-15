# System Definition Engine — Roadmap

> Last updated: 2026-05-15
> Status: Active roadmap
> Target: A standalone, deterministic, reusable system/data definition compiler that can be used by Saga projects and by independent external projects without being locked to SagaEngine.
> Scope: Source language, schema model, validation, canonical IR generation, graph IR, deterministic artifact emission, diagnostics, source maps, dependency manifests, incremental compilation, CLI, compiler facade, packaging, external integration contracts, and Saga-specific consumption through explicit artifact/schema boundaries.

---

## 0. Roadmap Convention

* `[x]` — Shipped. The note after the item names the files, modules, tests, packages, commands, or integration points that represent completed work.
* `[ ]` — Open. The item describes the finished production state, not temporary scaffolding.
* Shipped SDE work must name concrete implementation files, package outputs, tests, CLI commands, and integration evidence where practical.
* Open SDE work must describe stable compiler behavior, not temporary engine/editor scaffolding.
* SDE must remain standalone.
* SDE must remain deterministic.
* SDE must be usable outside SagaEngine.
* SDE must not depend on Saga, SagaEngine, SagaEditor, SagaServer, SagaShared, SagaCollaboration, Forge, Prism, SagaTools, asset pipeline implementation, or scripting host implementation.
* SDE may emit stable artifacts consumed by those systems through documented output formats.
* Saga-specific behavior must be represented through schemas, packages, manifests, adapters, or external consumers—not through hidden SDE dependencies on Saga modules.

SDE's job is not to become SagaEngine in compiler form.

SDE's job is to compile structured system definitions into deterministic artifacts.

That distinction is not cosmetic. It is the difference between a reusable tool and a captive subsystem.

---

## 1. Document Purpose

This document defines the roadmap for the System Definition Engine, also called SDE.

SDE is a standalone deterministic compiler for system/data definitions.

It exists to turn source definitions into validated, stable, reproducible, machine-consumable artifacts.

SDE owns:

* source language definition,
* lexical analysis,
* parsing,
* AST construction,
* schema declaration model,
* type system,
* symbol resolution,
* semantic validation,
* generic graph model validation,
* canonical IR generation,
* deterministic artifact emission,
* source maps,
* structured diagnostics,
* dependency manifests,
* incremental compile planning,
* stable CLI behavior,
* public compiler facade,
* packaging and distribution as an independent tool/library.

SDE does not own:

* Saga product shell,
* SagaEditor UI,
* SagaEngine runtime execution,
* SagaServer authority implementation,
* SagaCollaboration implementation,
* Forge build workflow ownership,
* Prism code intelligence implementation,
* SagaTools dispatch behavior,
* asset import/cook implementation,
* C# scripting compiler/host implementation,
* runtime gameplay execution,
* server session policy,
* database/persistence implementation,
* editor graph canvas implementation.

Correct model:

```txt
SDE source files
      ↓
SDE parser
      ↓
SDE semantic validator
      ↓
Canonical IR
      ↓
Deterministic artifacts + manifests + diagnostics
      ↓
Any consumer reads documented outputs
```

Saga-specific consumption model:

```txt
Saga project .sde files
      ↓
SDE compiler
      ↓
Generic artifacts + Saga schema-defined artifacts
      ↓
Forge packages outputs
      ↓
SagaEditor / Runtime / Server consume manifests/artifacts
```

Incorrect model:

```txt
SDE includes SagaEngine headers and becomes a secret engine subsystem.
```

That would make SDE look powerful for three weeks and unusable outside the engine forever.

---

## 2. Companion Documents

| Document                                      | Purpose                                                                        |
| --------------------------------------------- | ------------------------------------------------------------------------------ |
| `TOOLS_ROADMAP.md`                            | Tool ecosystem ownership index                                                 |
| `Tools/SystemDefinitionEngine/SDE_ROADMAP.md` | SDE compiler roadmap                                                           |
| `FORGE_ROADMAP.md`                            | Build workflow frontend and SDE invocation as a build step                     |
| `PRISM_ROADMAP.md`                            | Code/artifact intelligence and stale generated-output analysis                 |
| `SAGATOOLS_ROADMAP.md`                        | Thin command dispatcher that may invoke SDE                                    |
| `SHARED_ROADMAP.md`                           | Shared artifact/diagnostic/package contracts consumed by Saga systems          |
| `ENGINE_ROADMAP.md`                           | Runtime/server consumption of compiled SDE outputs and manifests               |
| `EDITOR_ROADMAP.md`                           | Editor-side SDE integration through service/tool boundaries                    |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md`              | Saga-specific gameplay graph consumer model built on SDE graph artifacts       |
| `AUTHORING_AUTHORITY_MODEL.md`                | Saga-specific authority metadata and validation consumption model              |
| `SAGA_SCRIPTING_ROADMAP.md`                   | Script/generated-code integration consuming SDE graph outputs where applicable |
| `ASSET_PIPELINE_ROADMAP.md`                   | Asset/artifact references and build pipeline interaction                       |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md`           | Forge/SDE/script/asset/package/publish workflow                                |
| `DependencyGraph.md`                          | Dependency ownership rules                                                     |

---

## 3. Fundamental Product Rule

* [ ] Keep SDE reusable outside SagaEngine.

  Done means an external project can:

  * clone or package SDE independently,
  * build SDE without SagaEngine source,
  * run `sde validate` on its own `.sde` files,
  * run `sde compile` to produce deterministic artifacts,
  * consume diagnostics, manifests, source maps, and generated outputs,
  * define its own schema packages,
  * use SDE without linking Saga modules.

* [ ] Keep Saga-specific behavior outside SDE core implementation.

  Done means Saga-specific concepts such as:

  * MMO authority,
  * quest reward graphs,
  * inventory semantics,
  * server-only gameplay,
  * replication policy,
  * Saga editor profiles,
  * Saga asset package layout,
  * Saga script binding policy,

  are represented by external schemas, manifests, adapter code, or consumer validation—not hardcoded dependencies in SDE core.

* [ ] Make Saga a consumer, not the owner of SDE.

  Done means Saga/Forge/Editor/Runtime/Server consume SDE through:

  * CLI,
  * public compiler facade,
  * artifact manifests,
  * diagnostics JSON,
  * source maps,
  * packaged library APIs where approved,
  * documented output formats.

  They do not consume:

  * SDE AST internals,
  * parser internals,
  * semantic analyzer internals,
  * compiler pass internals,
  * private IR builder implementation.

---

## 4. Ownership Boundary

* [x] Define SDE as a standalone deterministic compiler.

  Represented by:

  ```txt
  Tools/SystemDefinitionEngine/SDE_ROADMAP.md
  docs/roadmaps/TOOLS_ROADMAP.md
  DependencyGraph.md
  SHARED_ROADMAP.md
  ENGINE_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  SDE produces deterministic outputs.
  Other systems consume SDE outputs.
  SDE does not depend on Saga modules.
  ```

* [x] Provide standalone packaging and `SDE::Core` target.

  Represented by:

  ```txt
  Tools/SystemDefinitionEngine/CMakeLists.txt
  Tools/SystemDefinitionEngine/conanfile.py
  Tools/SystemDefinitionEngine/build.py
  Tools/SystemDefinitionEngine/version.json
  Tools/SystemDefinitionEngine/CHANGELOG.md
  SDE::Core
  SDE::SDE compatibility alias
  ```

  Preserved decision:

  ```txt
  SDE can be packaged and consumed independently.
  SagaEngine consumes SDE through find_package / packaged dependency boundaries.
  ```

* [x] Add project-level compiler facade surfaces.

  Represented by:

  ```txt
  SharedRegistrySet
  CompilerSession
  CompileContext
  cooperative cancellation token
  structured diagnostics
  stable hashing surfaces
  dependency manifests
  ```

  Preserved decision:

  ```txt
  Integrations use public compiler surfaces, not private parser/semantic internals.
  ```

* [ ] Keep SDE implementation owned by `Tools/SystemDefinitionEngine` or its extracted standalone repository.

  Done means lexer, parser, AST, semantic analyzer, IR builder, compiler passes, artifact writers, source maps, diagnostics, and tests live under SDE-owned paths.

Expected location while inside Saga monorepo:

```txt
Tools/SystemDefinitionEngine/
```

Expected external extraction model:

```txt
SystemDefinitionEngine/
  include/SDE/
  src/
  tests/
  docs/
  CMakeLists.txt
  conanfile.py
  build.py
  CHANGELOG.md
  LICENSE
  version.json
```

---

## 5. Dependency Rules

### 5.1 Allowed Dependencies

* [ ] Allow SDE to depend only on standalone compiler-safe libraries.

  Allowed examples:

  ```txt
  C++ standard library or Rust standard library
  platform-neutral filesystem utilities
  parser libraries if explicitly approved
  serialization libraries if explicitly approved
  hashing libraries if explicitly approved
  test framework
  small CLI parsing library if explicitly approved
  ```

* [ ] Allow SDE outputs to be consumed by other systems.

  Allowed output consumers:

  ```txt
  any external project
  Saga product shell
  SagaEditor
  SagaEngine runtime
  SagaServer
  SagaCollaboration
  Forge
  Prism
  SagaTools
  asset pipeline tools
  scripting tools
  CI/build scripts
  external build systems
  external editor integrations
  ```

* [ ] Allow optional adapters outside SDE core.

  Done means Saga-specific or external-project-specific adapters may exist outside SDE core implementation, for example:

  ```txt
  Saga integration adapter
  Forge SDE invocation step
  Prism SDE manifest reader
  Editor SDE diagnostics adapter
  external project SDE adapter
  ```

  These adapters consume SDE outputs or public APIs; they do not make SDE depend on them.

---

### 5.2 Forbidden Dependencies

* [ ] Prevent SDE from depending on Saga modules.

  Forbidden:

  ```txt
  SDE → Saga
  SDE → SagaEngine
  SDE → SagaEditor
  SDE → SagaServer
  SDE → SagaShared
  SDE → SagaCollaboration
  SDE → Forge
  SDE → Prism
  SDE → SagaTools
  SDE → AssetPipeline implementation
  SDE → Scripting implementation
  ```

* [ ] Prevent SDE from depending on editor/runtime/server implementation details.

  Forbidden examples:

  ```txt
  SDE includes ECS runtime headers
  SDE includes editor graph panel headers
  SDE includes runtime asset registry internals
  SDE includes collaboration session headers
  SDE includes Qt headers
  SDE includes server authority implementation
  SDE includes C# scripting host implementation
  ```

* [ ] Prevent SDE from becoming a runtime service.

  Done means SDE is invoked as a compiler/tool/library during authoring/build/CI workflows, not embedded as hidden gameplay execution logic in runtime/server loops.

A compiler can be called by tools.

It should not move into the runtime and start making lifestyle choices.

---

## 6. Core Design Principle: Core + Schema Packages + Consumers

SDE must be designed as a generic compiler core plus schema packages and external consumers.

Correct layering:

```txt
SDE Core
  source language
  parser
  generic schema model
  generic graph model
  generic validation engine
  canonical IR
  artifacts
  diagnostics

Schema Packages
  domain definitions
  attributes
  validation declarations
  artifact schemas

Consumers
  Saga / external tools / CI / editor integrations
  interpret artifacts according to their domain
```

Incorrect layering:

```txt
SDE Core
  knows Saga inventory
  knows MMO authority
  knows runtime package layout
  knows editor panels
  knows server replication implementation
```

That would make SDE very useful to one project and fundamentally less valuable everywhere else.

---

## 7. CLI Roadmap

* [ ] Provide stable SDE command-line interface.

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

* [ ] Provide `sde validate`.

  Done means:

  * source files are parsed,
  * schema validity is checked,
  * semantic errors are reported,
  * no output artifact is emitted unless explicitly requested,
  * exit code reflects validation success/failure,
  * diagnostics are available in human-readable and machine-readable forms.

* [ ] Provide `sde compile`.

  Done means:

  * source files are parsed,
  * semantic validation runs,
  * canonical IR is generated,
  * deterministic artifacts are emitted,
  * source maps are emitted,
  * diagnostics are emitted,
  * dependency manifest is emitted,
  * artifact manifest is emitted,
  * exit code reflects compile success/failure.

* [ ] Provide `sde inspect`.

  Done means users can inspect:

  * source structure,
  * parsed AST summary,
  * symbol table summary,
  * canonical IR summary,
  * graph summary,
  * artifact manifest,
  * dependency graph,
  * hash outputs.

* [ ] Provide `sde format`.

  Done means SDE can format source files deterministically according to documented style rules.

* [ ] Provide `sde doctor`.

  Done means doctor checks:

  * compiler executable health,
  * config validity,
  * output directory access,
  * cache directory access,
  * supported schema version,
  * package registry access where configured,
  * basic parse/compile smoke path.

* [ ] Keep CLI useful outside Saga.

  Done means CLI examples and tests include non-Saga sample projects.

---

## 8. Public Compiler Facade

* [x] Add public compiler facade concepts.

  Represented by:

  ```txt
  SharedRegistrySet
  CompilerSession
  CompileContext
  cancellation token
  structured diagnostics
  stable hashing surfaces
  ```

* [ ] Stabilize public compiler facade.

  Done means external integrations can:

  * create compiler sessions,
  * provide source roots,
  * provide schema packages,
  * configure output directories,
  * run validate/compile/inspect operations,
  * receive diagnostics,
  * receive manifests,
  * cancel compilation cooperatively.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Compiler/CompilerSession.hpp
Tools/SystemDefinitionEngine/include/SDE/Compiler/CompileContext.hpp
Tools/SystemDefinitionEngine/include/SDE/Compiler/CompileOptions.hpp
Tools/SystemDefinitionEngine/include/SDE/Compiler/CompileResult.hpp
Tools/SystemDefinitionEngine/include/SDE/Compiler/CancellationToken.hpp
Tools/SystemDefinitionEngine/src/Compiler/CompilerSession.cpp
```

* [ ] Keep public facade independent from Saga contracts.

  Done means facade types do not include `SagaShared`, `SagaEngine`, `SagaEditor`, `SagaServer`, or tool-specific headers.

---

## 9. Source Language and Schema Model

* [ ] Define the SDE source language.

  Done means the source language has documented:

  * file extension,
  * lexical rules,
  * comments,
  * identifiers,
  * literals,
  * type references,
  * declarations,
  * imports/includes,
  * attributes,
  * versioning rules,
  * graph syntax,
  * validation rule syntax where supported.

Expected docs:

```txt
Tools/SystemDefinitionEngine/docs/SDE_LANGUAGE.md
```

* [ ] Define generic schema declaration model.

  Done means SDE can represent domain-neutral declarations such as:

  * package,
  * import,
  * schema,
  * model,
  * field,
  * enum,
  * resource,
  * event,
  * message,
  * graph,
  * node,
  * edge,
  * validation rule,
  * artifact output.

* [ ] Keep schema system domain-extensible.

  Done means external projects can define their own schema packages without modifying SDE core.

Example external domains:

```txt
Saga gameplay schemas
editor customization schemas
build artifact schemas
asset metadata schemas
IoT device configuration schemas
simulation scenario schemas
workflow automation schemas
backend service config schemas
```

* [ ] Define import and dependency rules.

  Done means:

  * source files can reference other source files,
  * packages can import packages,
  * cycles are detected,
  * duplicate declarations are rejected,
  * ambiguous names fail clearly,
  * dependency graph is deterministic.

---

## 10. Lexer and Parser

* [ ] Add lexer.

  Done means lexer supports:

  * tokens,
  * identifiers,
  * keywords,
  * string literals,
  * numeric literals,
  * boolean literals,
  * comments,
  * source locations,
  * error recovery where practical.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Lexer/Token.hpp
Tools/SystemDefinitionEngine/include/SDE/Lexer/Lexer.hpp
Tools/SystemDefinitionEngine/src/Lexer/Lexer.cpp
```

* [ ] Add parser.

  Done means parser supports:

  * source files,
  * declarations,
  * attributes,
  * type references,
  * imports,
  * schema definitions,
  * graph definitions,
  * validation rule declarations,
  * error recovery,
  * source ranges for diagnostics.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Parser/Parser.hpp
Tools/SystemDefinitionEngine/include/SDE/Parser/ParseResult.hpp
Tools/SystemDefinitionEngine/src/Parser/Parser.cpp
```

* [ ] Add parser tests.

  Required coverage:

  * valid files,
  * invalid syntax,
  * missing braces,
  * invalid identifiers,
  * nested declarations,
  * import statements,
  * attribute syntax,
  * graph declarations,
  * diagnostic source ranges.

---

## 11. AST Model

* [ ] Define SDE AST.

  Done means AST can represent:

  * file unit,
  * declarations,
  * attributes,
  * type references,
  * imports,
  * graph declarations,
  * node declarations,
  * edge declarations,
  * expressions where needed,
  * source locations.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/AST/AstNode.hpp
Tools/SystemDefinitionEngine/include/SDE/AST/AstFile.hpp
Tools/SystemDefinitionEngine/include/SDE/AST/AstDeclaration.hpp
Tools/SystemDefinitionEngine/include/SDE/AST/AstTypeRef.hpp
Tools/SystemDefinitionEngine/include/SDE/AST/AstGraph.hpp
Tools/SystemDefinitionEngine/src/AST/AstPrinter.cpp
```

* [ ] Keep AST compiler-internal.

  Done means AST is not exported as a public Saga runtime/editor contract.

  Forbidden:

  ```txt
  SagaEngine includes SDE AST headers
  SagaEditor includes SDE AST headers
  SagaShared wraps SDE AST nodes
  Forge relies on SDE AST internals
  Prism relies on SDE AST internals
  ```

* [ ] Add AST debug printer.

  Done means `sde inspect` can print stable AST summaries for debugging and tests.

---

## 12. Type System

* [ ] Define generic SDE type system.

  Done means SDE supports:

  * primitive types,
  * named types,
  * enums,
  * arrays/lists,
  * maps/dictionaries where approved,
  * optional values,
  * references,
  * resource refs,
  * graph refs,
  * artifact refs,
  * versioned schema refs.

* [ ] Keep domain-specific types schema-defined.

  Done means types such as:

  ```txt
  PlayerRef
  ItemId
  QuestId
  Replicated<T>
  ServerEvent<T>
  ClientEvent<T>
  ```

  can be defined by Saga schema packages or consumer contracts, not hardcoded into SDE core.

* [ ] Add type compatibility validation.

  Done means validator catches:

  * unknown type,
  * invalid generic arity,
  * invalid reference type,
  * incompatible field value,
  * incompatible graph pin connection,
  * incompatible artifact reference.

---

## 13. Semantic Analysis

* [ ] Add symbol table.

  Done means SDE can resolve:

  * declarations,
  * imports,
  * type names,
  * package names,
  * resource references,
  * graph node references,
  * schema references,
  * artifact references.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Semantic/Symbol.hpp
Tools/SystemDefinitionEngine/include/SDE/Semantic/SymbolTable.hpp
Tools/SystemDefinitionEngine/src/Semantic/SymbolTable.cpp
```

* [ ] Add semantic validator.

  Done means validator catches:

  * duplicate definitions,
  * unknown references,
  * invalid type usage,
  * invalid graph connections,
  * invalid attributes,
  * dependency cycles,
  * incompatible schema versions,
  * invalid artifact refs,
  * invalid validation rule use.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Semantic/SemanticValidator.hpp
Tools/SystemDefinitionEngine/include/SDE/Semantic/SemanticDiagnostic.hpp
Tools/SystemDefinitionEngine/src/Semantic/SemanticValidator.cpp
```

* [ ] Add semantic tests.

  Required coverage:

  * valid schema,
  * duplicate declarations,
  * unknown type,
  * invalid attribute,
  * invalid graph edge,
  * dependency cycle,
  * incompatible version,
  * invalid artifact ref.

---

## 14. Validation Rule Model

* [ ] Define generic validation rule model.

  Done means SDE can validate constraints expressed by schemas without hardcoding consumer-specific semantics into compiler core.

Examples of generic rules:

```txt
required field
unique value
reference exists
type compatibility
allowed enum value
value range
string pattern
graph edge type compatibility
acyclic dependency
artifact kind compatibility
version compatibility
```

* [ ] Support schema-defined validation rules.

  Done means schema packages can define validation expectations that SDE can apply generically.

* [ ] Keep consumer-specific policy outside SDE core when not representable generically.

  Example:

  ```txt
  SDE may validate that a graph node has an attribute named authority and that its value is one of the schema-defined enum values.
  SDE core should not hardcode Saga's MMO server authority implementation.
  Saga/Forge/Runtime/Server may apply additional policy using SDE output manifests.
  ```

* [ ] Emit structured validation diagnostics.

  Done means validation diagnostics include:

  * code,
  * severity,
  * source range,
  * schema path,
  * declaration path,
  * reference path,
  * rule id,
  * metadata.

---

## 15. Canonical IR

* [ ] Define canonical IR.

  Done means IR is:

  * deterministic,
  * serializable,
  * source-mapped,
  * schema-aware,
  * versioned,
  * independent from AST internals,
  * independent from Saga modules,
  * stable enough for external consumers.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/IR/IrModule.hpp
Tools/SystemDefinitionEngine/include/SDE/IR/IrDeclaration.hpp
Tools/SystemDefinitionEngine/include/SDE/IR/IrType.hpp
Tools/SystemDefinitionEngine/include/SDE/IR/IrValue.hpp
Tools/SystemDefinitionEngine/src/IR/IrBuilder.cpp
```

* [ ] Define deterministic ordering.

  Done means identical semantic inputs produce identical IR ordering and hashes.

* [ ] Define IR versioning.

  Done means IR consumers can reject unsupported IR versions clearly.

* [ ] Keep canonical IR generic.

  Done means Saga-specific IR interpretation happens in Saga schemas/consumers, not in SDE core.

---

## 16. Graph IR

* [ ] Define generic graph model.

  Done means SDE can represent:

  * graph id,
  * graph kind,
  * nodes,
  * pins,
  * edges,
  * attributes,
  * metadata,
  * source locations,
  * graph dependencies,
  * artifact outputs.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Graph/GraphIr.hpp
Tools/SystemDefinitionEngine/include/SDE/Graph/GraphNode.hpp
Tools/SystemDefinitionEngine/include/SDE/Graph/GraphPin.hpp
Tools/SystemDefinitionEngine/include/SDE/Graph/GraphEdge.hpp
Tools/SystemDefinitionEngine/include/SDE/Graph/GraphValidator.hpp
Tools/SystemDefinitionEngine/src/Graph/GraphIrBuilder.cpp
Tools/SystemDefinitionEngine/src/Graph/GraphValidator.cpp
```

* [ ] Keep graph model domain-neutral.

  Done means SDE graph IR does not hardcode Saga gameplay concepts like:

  * quest reward,
  * inventory transaction,
  * server-only block,
  * replicated property,
  * prediction buffer.

  Those can be represented as schema-defined attributes, block descriptors, artifact metadata, or Saga consumer validation.

* [ ] Support graph source maps.

  Done means graph diagnostics can point to:

  * graph declaration,
  * node declaration,
  * pin declaration,
  * edge declaration,
  * attribute source range.

* [ ] Support graph artifact emission.

  Done means SDE can emit deterministic graph artifacts and manifests for consumers.

---

## 17. Artifact Emission

* [ ] Define artifact manifest format.

  Done means artifact manifests describe:

  * artifact id,
  * artifact kind,
  * schema id,
  * source files,
  * source hashes,
  * output paths,
  * artifact hashes,
  * dependency refs,
  * compiler version,
  * schema versions,
  * diagnostics summary.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Artifacts/ArtifactManifest.hpp
Tools/SystemDefinitionEngine/include/SDE/Artifacts/ArtifactWriter.hpp
Tools/SystemDefinitionEngine/src/Artifacts/ArtifactManifestWriter.cpp
```

* [ ] Emit deterministic compiled graph artifacts.

  Done means graph artifacts are stable for identical semantic inputs.

* [ ] Emit source maps.

  Done means generated artifacts can map back to source locations.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/SourceMap/SourceMap.hpp
Tools/SystemDefinitionEngine/include/SDE/SourceMap/SourceMapWriter.hpp
Tools/SystemDefinitionEngine/src/SourceMap/SourceMapWriter.cpp
```

* [ ] Emit dependency manifests.

  Done means consumers can know which source/schema/artifact inputs affected outputs.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Dependency/DependencyManifest.hpp
Tools/SystemDefinitionEngine/include/SDE/Dependency/DependencyGraph.hpp
Tools/SystemDefinitionEngine/src/Dependency/DependencyManifestWriter.cpp
```

* [ ] Keep artifact format documented.

  Expected docs:

```txt
Tools/SystemDefinitionEngine/docs/SDE_ARTIFACT_FORMAT.md
Tools/SystemDefinitionEngine/docs/SDE_SOURCE_MAP_FORMAT.md
Tools/SystemDefinitionEngine/docs/SDE_DEPENDENCY_MANIFEST.md
```

---

## 18. Diagnostics

* [x] Add structured diagnostic categories, source ranges, deterministic sorting, and metadata.

  Represented by:

  ```txt
  structured diagnostic categories
  source ranges
  deterministic sorting
  metadata for editor/CI integration
  ```

* [ ] Stabilize diagnostic model.

  Done means diagnostics include:

  * diagnostic code,
  * severity,
  * message,
  * source file,
  * source range,
  * rule id,
  * schema path,
  * declaration path,
  * metadata,
  * optional suggested fix.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Diagnostics/Diagnostic.hpp
Tools/SystemDefinitionEngine/include/SDE/Diagnostics/DiagnosticCode.hpp
Tools/SystemDefinitionEngine/include/SDE/Diagnostics/DiagnosticSeverity.hpp
Tools/SystemDefinitionEngine/include/SDE/Diagnostics/DiagnosticSink.hpp
Tools/SystemDefinitionEngine/src/Diagnostics/DiagnosticFormatter.cpp
```

* [ ] Emit diagnostics in multiple formats.

  Required formats:

  ```txt
  human-readable console
  JSON
  machine-readable report suitable for editor/CI/build integration
  ```

* [ ] Keep diagnostics generic but extensible.

  Done means Saga-specific diagnostic categories may appear through schema/package names or consumer metadata, but SDE core remains reusable.

---

## 19. Incremental Compilation

* [ ] Add incremental compilation planning.

  Done means SDE can determine affected outputs based on:

  * source file hashes,
  * schema package hashes,
  * dependency manifests,
  * compiler version,
  * compile options,
  * output artifact versions.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Incremental/IncrementalCompilePlan.hpp
Tools/SystemDefinitionEngine/include/SDE/Incremental/CompileCache.hpp
Tools/SystemDefinitionEngine/src/Incremental/IncrementalPlanner.cpp
```

* [ ] Reject unsafe incremental reuse.

  Done means cached outputs are invalidated when:

  * source changes,
  * imported package changes,
  * schema changes,
  * compiler version changes,
  * compile options change,
  * artifact format version changes.

* [ ] Emit incremental build diagnostics/report.

  Done means users can inspect why files were rebuilt or skipped.

---

## 20. Formatting

* [ ] Add deterministic formatter.

  Done means `sde format` can format source files consistently.

* [ ] Define style rules.

  Done means formatting behavior is documented for:

  * indentation,
  * braces,
  * attributes,
  * imports,
  * graph declarations,
  * comments preservation where practical,
  * trailing commas where supported.

Expected docs:

```txt
Tools/SystemDefinitionEngine/docs/SDE_FORMATTING.md
```

---

## 21. Package and Schema Distribution

* [ ] Define schema package model.

  Done means schema packages can describe:

  * package name,
  * package version,
  * exported schemas,
  * exported types,
  * exported enums,
  * validation rules,
  * artifact schemas,
  * dependency packages.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Packages/SchemaPackage.hpp
Tools/SystemDefinitionEngine/include/SDE/Packages/PackageRegistry.hpp
Tools/SystemDefinitionEngine/src/Packages/PackageRegistry.cpp
```

* [ ] Support local schema packages.

  Done means projects can define local schema packages without modifying SDE core.

* [ ] Support installed schema packages.

  Done means external projects can reuse schema packages across workspaces.

* [ ] Keep Saga schema packages outside SDE core.

  Done means Saga can ship schema packages such as:

  ```txt
  saga.gameplay
  saga.editor
  saga.assets
  saga.authority
  saga.scripting
  ```

  but SDE core does not depend on Saga modules.

---

## 22. External Integration Model

* [ ] Document external integration methods.

  Done means external users can integrate SDE through:

  * CLI,
  * CMake package target,
  * Conan package,
  * public compiler facade,
  * artifact manifests,
  * diagnostics JSON,
  * source maps,
  * dependency manifests.

Expected docs:

```txt
Tools/SystemDefinitionEngine/docs/SDE_INTEGRATION.md
Tools/SystemDefinitionEngine/docs/SDE_CMAKE_USAGE.md
Tools/SystemDefinitionEngine/docs/SDE_CONAN_USAGE.md
```

* [ ] Provide non-Saga sample projects.

  Done means repository includes at least one sample unrelated to SagaEngine.

Expected examples:

```txt
Tools/SystemDefinitionEngine/examples/basic_config/
Tools/SystemDefinitionEngine/examples/workflow_graph/
Tools/SystemDefinitionEngine/examples/external_schema_package/
```

* [ ] Provide compatibility policy.

  Done means SDE documents:

  * source language stability,
  * artifact format stability,
  * CLI stability,
  * public API stability,
  * semantic versioning guarantees.

---

## 23. Saga Integration Boundary

SDE supports Saga by producing artifacts Saga consumes.

SDE must not become Saga-specific.

* [ ] Define Saga schema packages as external SDE inputs.

  Done means Saga-specific schemas are expressed as SDE packages, not SDE compiler code.

Examples:

```txt
saga.gameplay.schema.sde
saga.editor.schema.sde
saga.assets.schema.sde
saga.authority.schema.sde
saga.scripting.schema.sde
```

* [ ] Define Saga artifact output contracts through documented formats.

  Done means Saga consumes:

  * graph artifacts,
  * source maps,
  * dependency manifests,
  * diagnostics reports,
  * artifact manifests,
  * schema manifests.

* [ ] Keep Saga-specific validation policy layered.

  Done means SDE can validate what is expressible generically through schemas/rules, while Saga/Forge/Runtime/Server may apply additional policy using emitted metadata.

Example:

```txt
SDE validates:
- graph has authority attribute from schema enum
- block reference exists
- pin types are compatible
- artifact reference exists

Saga/Forge/Server validate:
- this server-only graph is not packaged as executable client logic
- this persistent write is allowed only in server package
- this graph/script authority manifest matches package destination
```

* [ ] Provide Saga integration tests without Saga dependency inside SDE.

  Done means tests can use sample Saga-like schema packages as data fixtures, not include Saga source headers.

---

## 24. Build and Packaging

* [x] Ship SDE as independent CMake/Conan package.

  Represented by:

  ```txt
  SDE::Core
  conanfile.py
  build.py
  SDEConfig.cmake
  SDEConfigVersion.cmake
  SDETargets.cmake
  ```

* [ ] Keep SDE repo-extractable.

  Done means copying `Tools/SystemDefinitionEngine/` to a new repository root requires no structural rewrite.

* [ ] Add installable CLI.

  Done means installing SDE provides:

  ```txt
  sde executable
  SDE library/package target
  docs
  examples
  license
  changelog
  version metadata
  ```

* [ ] Add package compatibility tests.

  Required coverage:

  * CMake configure with `find_package(SDE CONFIG REQUIRED)`,
  * Conan package creation,
  * downstream sample linking against `SDE::Core`,
  * CLI runs after install,
  * no Saga paths required.

---

## 25. Prism and Forge Interaction Boundary

### 25.1 Forge

* [ ] Support Forge invoking SDE as a compiler step.

  Done means Forge can run:

  * `sde validate`,
  * `sde compile`,
  * consume artifact manifests,
  * consume diagnostics,
  * consume dependency manifests,
  * fail build according to diagnostic severity/profile.

* [ ] Keep Forge outside SDE internals.

  Forbidden:

  ```txt
  Forge includes SDE AST internals
  Forge includes SDE parser internals
  Forge depends on SDE semantic analyzer internals
  ```

### 25.2 Prism

* [ ] Support Prism reading SDE output manifests.

  Done means Prism can consume:

  * artifact manifests,
  * source maps,
  * dependency manifests,
  * generated code manifests,
  * diagnostics reports.

* [ ] Keep Prism outside SDE internals.

  Forbidden:

  ```txt
  Prism includes SDE AST internals
  Prism runs SDE parser as hidden indexing truth unless through public facade/CLI
  Prism depends on SDE private IR builder internals
  ```

---

## 26. Testing Strategy

### 26.1 Unit Tests

* [ ] Add unit tests for compiler subsystems.

  Required coverage:

  * lexer,
  * parser,
  * AST construction,
  * symbol table,
  * semantic validation,
  * type system,
  * graph validation,
  * IR builder,
  * artifact writer,
  * source maps,
  * dependency manifests,
  * diagnostics formatting.

---

### 26.2 Golden Output Tests

* [ ] Add golden artifact tests.

  Done means stable inputs produce stable expected outputs for:

  * canonical IR,
  * graph artifacts,
  * artifact manifests,
  * source maps,
  * dependency manifests,
  * diagnostics reports.

---

### 26.3 Determinism Tests

* [ ] Add determinism tests.

  Required coverage:

  * repeated compile produces identical artifact hashes,
  * file order does not affect output semantics,
  * map/object ordering is stable,
  * diagnostics ordering is stable,
  * dependency manifest ordering is stable.

---

### 26.4 Package/External Use Tests

* [ ] Add standalone package tests.

  Required coverage:

  * build SDE alone,
  * install SDE alone,
  * consume SDE through CMake package,
  * consume SDE through Conan package,
  * run CLI outside Saga repo,
  * compile non-Saga sample project.

---

### 26.5 Boundary Tests

* [ ] Add dependency boundary tests.

  Required checks:

  * SDE does not include Saga headers,
  * SDE does not include SagaEngine headers,
  * SDE does not include SagaEditor headers,
  * SDE does not include SagaServer headers,
  * SDE does not include SagaShared headers,
  * SDE does not include SagaCollaboration headers,
  * SDE does not include Forge headers,
  * SDE does not include Prism headers,
  * SDE does not include SagaTools headers,
  * SDE does not include Qt headers.

---

## 27. MVP Vertical Slice

The first production SDE vertical slice should prove standalone usefulness and Saga usefulness without coupling them.

Required scenario A: external standalone project

```txt
external workflow graph sample
    ↓
sde validate
    ↓
sde compile
    ↓
canonical IR + graph artifact + diagnostics + source map + dependency manifest
```

Required behavior:

1. SDE builds independently.
2. `sde validate` validates a non-Saga sample project.
3. `sde compile` emits deterministic artifacts.
4. Repeated compile produces identical artifact hashes.
5. Diagnostics are machine-readable.
6. Sample consumer reads artifact manifest.
7. No Saga headers or paths are required.

Required scenario B: Saga-like schema package as external input

```txt
saga-like schema package fixture
    ↓
quest reward graph source
    ↓
sde compile
    ↓
generic graph artifact + schema-defined authority metadata + source map
```

Required behavior:

1. Saga-like schema is loaded as data/package.
2. SDE validates schema-defined attributes and graph structure.
3. SDE emits graph artifact and metadata.
4. SDE does not include Saga modules.
5. A separate Saga/Forge-side test consumes the output and applies Saga-specific authority/package policy.

This proves the correct split:

```txt
SDE compiles definitions.
Saga interprets Saga-specific meaning.
```

---

## 28. Non-Goals

SDE must not become:

* SagaEngine runtime subsystem,
* SagaEditor graph backend implementation,
* SagaServer authority engine,
* Forge build planner,
* Prism code indexer,
* SagaTools dispatcher,
* asset importer/cooker,
* C# compiler or scripting host,
* runtime gameplay VM,
* database migration engine unless defined as external schema/tool usage,
* product shell,
* editor UI,
* collaboration backend.

SDE can describe systems.

SDE should not secretly become every system it describes.

---

## 29. Risk Register

### 29.1 Risk: SDE Becomes Saga-Locked

Mitigation:

* no Saga module dependencies,
* non-Saga examples,
* standalone package tests,
* schema package model,
* output/manifests instead of direct engine integration.

---

### 29.2 Risk: SDE Becomes a General Programming Language

Mitigation:

* keep SDE focused on definitions, schemas, graphs, validation, artifacts,
* use C++/C#/runtime systems for algorithms/execution,
* avoid feature creep into general-purpose computation.

---

### 29.3 Risk: SDE Core Hardcodes Consumer Semantics

Mitigation:

* represent domain semantics through schemas/attributes/manifests,
* keep generic validation in core,
* apply consumer-specific policy outside SDE where needed.

---

### 29.4 Risk: External Integrations Depend on Internals

Mitigation:

* stable CLI,
* stable compiler facade,
* documented artifact formats,
* source maps,
* dependency manifests,
* private headers kept private.

---

### 29.5 Risk: Determinism Is Broken by Convenience

Mitigation:

* deterministic ordering,
* stable hashing,
* golden output tests,
* no unordered output emission,
* explicit compiler/tool/schema versions in manifests.

---

## 30. Suggested File Targets

Expected public compiler files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Compiler/CompilerSession.hpp
Tools/SystemDefinitionEngine/include/SDE/Compiler/CompileContext.hpp
Tools/SystemDefinitionEngine/include/SDE/Compiler/CompileOptions.hpp
Tools/SystemDefinitionEngine/include/SDE/Compiler/CompileResult.hpp
Tools/SystemDefinitionEngine/include/SDE/Compiler/CancellationToken.hpp
```

Expected language/compiler files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Lexer/Token.hpp
Tools/SystemDefinitionEngine/include/SDE/Lexer/Lexer.hpp
Tools/SystemDefinitionEngine/include/SDE/Parser/Parser.hpp
Tools/SystemDefinitionEngine/include/SDE/AST/AstNode.hpp
Tools/SystemDefinitionEngine/include/SDE/Semantic/SymbolTable.hpp
Tools/SystemDefinitionEngine/include/SDE/Semantic/SemanticValidator.hpp
Tools/SystemDefinitionEngine/include/SDE/IR/IrModule.hpp
Tools/SystemDefinitionEngine/include/SDE/Graph/GraphIr.hpp
Tools/SystemDefinitionEngine/include/SDE/Artifacts/ArtifactManifest.hpp
Tools/SystemDefinitionEngine/include/SDE/SourceMap/SourceMap.hpp
Tools/SystemDefinitionEngine/include/SDE/Dependency/DependencyManifest.hpp
```

Expected docs:

```txt
Tools/SystemDefinitionEngine/docs/SDE_LANGUAGE.md
Tools/SystemDefinitionEngine/docs/SDE_SCHEMA_MODEL.md
Tools/SystemDefinitionEngine/docs/SDE_GRAPH_MODEL.md
Tools/SystemDefinitionEngine/docs/SDE_ARTIFACT_FORMAT.md
Tools/SystemDefinitionEngine/docs/SDE_SOURCE_MAP_FORMAT.md
Tools/SystemDefinitionEngine/docs/SDE_DEPENDENCY_MANIFEST.md
Tools/SystemDefinitionEngine/docs/SDE_INTEGRATION.md
Tools/SystemDefinitionEngine/docs/SDE_CMAKE_USAGE.md
Tools/SystemDefinitionEngine/docs/SDE_CONAN_USAGE.md
```

Expected examples:

```txt
Tools/SystemDefinitionEngine/examples/basic_config/
Tools/SystemDefinitionEngine/examples/workflow_graph/
Tools/SystemDefinitionEngine/examples/external_schema_package/
Tools/SystemDefinitionEngine/examples/saga_like_schema_fixture/
```

Expected tests:

```txt
Tools/SystemDefinitionEngine/tests/LexerTests.cpp
Tools/SystemDefinitionEngine/tests/ParserTests.cpp
Tools/SystemDefinitionEngine/tests/SemanticTests.cpp
Tools/SystemDefinitionEngine/tests/GraphTests.cpp
Tools/SystemDefinitionEngine/tests/IrGoldenTests.cpp
Tools/SystemDefinitionEngine/tests/ArtifactGoldenTests.cpp
Tools/SystemDefinitionEngine/tests/DeterminismTests.cpp
Tools/SystemDefinitionEngine/tests/StandalonePackageTests.cpp
Tools/SystemDefinitionEngine/tests/BoundaryTests.cpp
```

---

## 31. Decision Summary

Preserve these decisions:

```txt
SDE is standalone.
SDE is deterministic.
SDE can be used outside SagaEngine.
SDE core does not depend on Saga modules.
SDE source language and schema model are domain-extensible.
Saga-specific behavior is modeled through schema packages, artifacts, manifests, and external consumers.
SDE owns parsing, validation, canonical IR, graph IR, source maps, diagnostics, dependency manifests, and deterministic artifact emission.
SDE does not own runtime execution, server authority, editor UI, build orchestration, code intelligence, asset import/cook, or scripting host behavior.
Forge invokes SDE.
Prism reads SDE outputs.
SagaEditor displays SDE diagnostics/artifacts.
Runtime/server consume packaged SDE outputs through manifests.
External projects can use SDE without Saga.
```

SDE should be strong enough to serve SagaEngine.

But it must not become trapped inside SagaEngine.

A tool that can stand alone is an asset.

A tool that only works because the engine is nearby is a dependency-shaped liability.
