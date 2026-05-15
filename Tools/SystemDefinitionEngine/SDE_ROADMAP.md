# System Definition Engine — Roadmap

> Last updated: 2026-05-14  
> Status: Active roadmap  
> Target: A standalone deterministic data compiler for Saga projects.  
> Scope: Schema parsing, validation, canonical IR generation, deterministic artifact emission, diagnostics, incremental compilation, CLI integration, and tool-facing outputs.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files, modules, or integration points that represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item describes the finished production state rather than interim scaffolding.
- Shipped items must name the files, modules, or integration points that represent the completed work.
- Open items must describe the finished state, not temporary scaffolding.
- SDE must remain standalone.
- SDE must be deterministic.
- SDE must not depend on Saga, SagaEngine, SagaEditor, SagaServer, SagaShared, SagaCollaboration, Forge, Prism, or SagaTools.
- SDE may emit outputs consumed by those systems through explicit artifact formats.

---

## 1. Document Purpose

This document defines the roadmap for the System Definition Engine, also called SDE.

SDE is Saga’s deterministic data compiler.

It exists to turn project/system definitions into validated, stable, runtime-consumable artifacts.

SDE owns:

- source definition parsing,
- schema validation,
- semantic validation,
- canonical IR generation,
- deterministic hashing,
- artifact generation,
- diagnostics,
- dependency tracking,
- incremental compile planning,
- command-line compiler behavior.

SDE does not own:

- Saga product shell,
- editor UI,
- runtime execution,
- server authority,
- collaboration implementation,
- build frontend UX,
- code intelligence database,
- tool dispatcher behavior.

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
Deterministic artifacts
      ↓
Saga / Editor / Runtime / Server / Tools consume outputs
```

Incorrect model:

```txt
SDE includes engine/editor headers and becomes a secret subsystem.
```

That would be convenient in the same way mixing every cable in one box is convenient until something catches fire.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `docs/roadmaps/TOOLS_ROADMAP.md` | Tool ecosystem ownership index |
| `Tools/SystemDefinitionEngine/SDE_ROADMAP.md` | SDE compiler roadmap |
| `Tools/Forge/FORGE_ROADMAP.md` | Forge build workflow frontend |
| `Tools/Prism/PRISM_ROADMAP.md` | Prism code intelligence |
| `Tools/SagaTools/SAGATOOLS_ROADMAP.md` | Thin command dispatcher |
| `SHARED_ROADMAP.md` | Shared contracts and artifact references |
| `ENGINE_ROADMAP.md` | Runtime/server consumption of SDE outputs |
| `EDITOR_ROADMAP.md` | Editor integration through service boundaries |
| `DependencyGraph.md` | Dependency ownership rules |

---

## 3. Ownership Boundary

- [x] Define SDE as a standalone deterministic compiler.

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

- [ ] Keep SDE independent from Saga modules.

  Done means SDE does not include headers from:

  ```txt
  Saga
  SagaEngine
  SagaEditor
  SagaServer
  SagaShared
  SagaCollaboration
  Forge
  Prism
  SagaTools
  ```

- [ ] Keep SDE implementation owned by `Tools/SystemDefinitionEngine`.

  Done means SDE parser, AST, semantic analyzer, IR builder, compiler passes, codegen, and diagnostics live under SDE-owned paths.

Expected location:

```txt
Tools/SystemDefinitionEngine/
```

---

## 4. Dependency Rules

### 4.1 Allowed Dependencies

- [ ] Allow SDE to depend only on standalone compiler-safe libraries.

  Allowed examples:

  ```txt
  C++ standard library or Rust standard library
  platform-neutral filesystem utilities
  parser libraries if explicitly approved
  serialization libraries if explicitly approved
  hashing libraries if explicitly approved
  test framework
  ```

- [ ] Allow SDE outputs to be consumed by other systems.

  Allowed output consumers:

  ```txt
  Saga product shell
  SagaEditor
  SagaEngine runtime
  SagaServer
  SagaCollaboration
  Forge
  Prism
  SagaTools
  CI/build scripts
  ```

---

### 4.2 Forbidden Dependencies

- [ ] Prevent SDE from depending on Saga modules.

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
  ```

- [ ] Prevent SDE from depending on editor/runtime implementation details.

  Forbidden examples:

  ```txt
  SDE includes ECS runtime headers
  SDE includes editor graph panel headers
  SDE includes runtime asset registry internals
  SDE includes collaboration session headers
  SDE includes Qt headers
  ```

- [ ] Prevent SDE from becoming a runtime service.

  Done means SDE is invoked as a compiler/tool, not embedded as runtime gameplay logic.

A compiler can be called by tools.

It should not move into the runtime and start making lifestyle choices.

---

## 5. CLI Roadmap

- [ ] Provide stable SDE command-line interface.

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

- [ ] Provide `sde validate`.

  Done means:

  - source files are parsed,
  - schema validity is checked,
  - semantic errors are reported,
  - no output artifact is emitted unless explicitly requested,
  - exit code reflects validation success/failure.

- [ ] Provide `sde compile`.

  Done means:

  - source files are parsed,
  - semantic validation runs,
  - canonical IR is generated,
  - deterministic artifacts are emitted,
  - diagnostics are emitted,
  - dependency manifest is emitted,
  - exit code reflects compile success/failure.

- [ ] Provide `sde inspect`.

  Done means users can inspect:

  - source structure,
  - parsed AST summary,
  - canonical IR summary,
  - artifact manifest,
  - dependency graph,
  - hash outputs.

- [ ] Provide `sde doctor`.

  Done means doctor checks:

  - compiler executable health,
  - config validity,
  - output directory access,
  - cache directory access,
  - supported schema version,
  - basic parse/compile smoke path.

---

## 6. Source Language and Schema Model

- [ ] Define the SDE source language.

  Done means the source language has documented:

  - file extension,
  - lexical rules,
  - comments,
  - identifiers,
  - literals,
  - type references,
  - declarations,
  - imports/includes,
  - versioning rules.

Expected docs:

```txt
Tools/SystemDefinitionEngine/docs/SDE_LANGUAGE.md
```

- [ ] Define schema declaration model.

  Done means SDE can represent:

  - systems,
  - components,
  - resources,
  - events,
  - messages,
  - packages,
  - graph nodes,
  - graph edges,
  - validation rules.

- [ ] Define import and dependency rules.

  Done means:

  - source files can reference other source files,
  - cycles are detected,
  - duplicate declarations are rejected,
  - ambiguous names fail clearly,
  - dependency graph is deterministic.

---

## 7. Lexer and Parser

- [ ] Add lexer.

  Done means lexer supports:

  - tokens,
  - identifiers,
  - keywords,
  - string literals,
  - numeric literals,
  - comments,
  - source locations,
  - error recovery where practical.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Lexer/Token.hpp
Tools/SystemDefinitionEngine/include/SDE/Lexer/Lexer.hpp
Tools/SystemDefinitionEngine/src/Lexer/Lexer.cpp
```

- [ ] Add parser.

  Done means parser supports:

  - source files,
  - declarations,
  - attributes,
  - type references,
  - imports,
  - graph definitions,
  - error recovery,
  - source ranges for diagnostics.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Parser/Parser.hpp
Tools/SystemDefinitionEngine/include/SDE/Parser/ParseResult.hpp
Tools/SystemDefinitionEngine/src/Parser/Parser.cpp
```

- [ ] Add parser tests.

  Required coverage:

  - valid files,
  - invalid syntax,
  - missing braces,
  - invalid identifiers,
  - nested declarations,
  - import statements,
  - diagnostic source ranges.

---

## 8. AST Model

- [ ] Define SDE AST.

  Done means AST can represent:

  - file unit,
  - declarations,
  - attributes,
  - type references,
  - imports,
  - graph declarations,
  - expressions where needed,
  - source locations.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/AST/AstNode.hpp
Tools/SystemDefinitionEngine/include/SDE/AST/AstFile.hpp
Tools/SystemDefinitionEngine/include/SDE/AST/AstDeclaration.hpp
Tools/SystemDefinitionEngine/include/SDE/AST/AstTypeRef.hpp
Tools/SystemDefinitionEngine/src/AST/AstPrinter.cpp
```

- [ ] Keep AST compiler-internal.

  Done means AST is not exported as a public Saga runtime/editor contract.

  Forbidden:

  ```txt
  SagaEngine includes SDE AST headers
  SagaEditor includes SDE AST headers
  SagaShared wraps SDE AST nodes
  ```

- [ ] Add AST debug printer.

  Done means `sde inspect` can print stable AST summaries for debugging and tests.

---

## 9. Semantic Analysis

- [ ] Add symbol table.

  Done means SDE can resolve:

  - declarations,
  - imports,
  - type names,
  - package names,
  - resource references,
  - graph node references,
  - schema references.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Semantic/Symbol.hpp
Tools/SystemDefinitionEngine/include/SDE/Semantic/SymbolTable.hpp
Tools/SystemDefinitionEngine/src/Semantic/SymbolTable.cpp
```

- [ ] Add semantic validator.

  Done means validator catches:

  - duplicate definitions,
  - unknown references,
  - invalid type usage,
  - invalid graph connections,
  - invalid attributes,
  - dependency cycles,
  - incompatible schema versions.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Semantic/SemanticValidator.hpp
Tools/SystemDefinitionEngine/include/SDE/Semantic/SemanticDiagnostic.hpp
Tools/SystemDefinitionEngine/src/Semantic/SemanticValidator.cpp
```

- [ ] Add semantic tests.

  Required coverage:

  - valid schema,
  - duplicate declarations,
  - unknown type,
  - invalid attribute,
  - invalid graph edge,
  - dependency cycle,
  - incompatible version.

---

## 10. Canonical IR

- [ ] Define canonical IR.

  Done means IR is:

  - deterministic,
  - order-stable,
  - normalized,
  - independent from source formatting,
  - suitable for hashing,
  - suitable for artifact generation,
  - suitable for inspection.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/IR/IrModule.hpp
Tools/SystemDefinitionEngine/include/SDE/IR/IrNode.hpp
Tools/SystemDefinitionEngine/include/SDE/IR/IrType.hpp
Tools/SystemDefinitionEngine/include/SDE/IR/IrGraph.hpp
Tools/SystemDefinitionEngine/src/IR/IrBuilder.cpp
```

- [ ] Add AST-to-IR lowering.

  Done means:

  - AST declarations lower into canonical IR,
  - imports are resolved,
  - names are canonicalized,
  - declaration ordering is stable,
  - source locations are preserved for diagnostics.

- [ ] Add IR validation.

  Done means IR validator catches:

  - invalid node references,
  - invalid type ids,
  - invalid dependency edges,
  - missing required metadata,
  - non-canonical ordering.

- [ ] Add IR snapshot tests.

  Done means known source inputs produce stable IR text or JSON snapshots.

---

## 11. Determinism Requirements

- [ ] Make compilation deterministic.

  Done means identical inputs produce identical outputs across:

  - repeated runs,
  - clean build directories,
  - different machines where supported,
  - different filesystem traversal order,
  - different thread scheduling.

- [ ] Add deterministic ordering rules.

  Done means SDE sorts or canonicalizes:

  - files,
  - declarations,
  - imports,
  - symbols,
  - graph nodes,
  - graph edges,
  - diagnostics where ordering matters,
  - artifact manifest entries.

- [ ] Add determinism tests.

  Required coverage:

  - same input produces same hash,
  - input file order does not affect output,
  - declaration order rules are stable,
  - generated artifact manifest is stable,
  - diagnostic order is stable.

If the compiler is not deterministic, every downstream cache becomes a gambling machine with better branding.

---

## 12. Hashing and Stable IDs

- [ ] Add stable hash generation.

  Done means SDE can produce deterministic hashes for:

  - source files,
  - canonical IR modules,
  - artifact outputs,
  - dependency graph,
  - schema definitions.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Hash/StableHash.hpp
Tools/SystemDefinitionEngine/include/SDE/Hash/HashManifest.hpp
Tools/SystemDefinitionEngine/src/Hash/StableHash.cpp
```

- [ ] Add stable id generation.

  Done means SDE can generate stable ids for:

  - schema ids,
  - component ids,
  - resource ids,
  - graph ids,
  - node ids,
  - artifact ids.

- [ ] Document id stability policy.

  Done means docs explain:

  - what changes an id,
  - what does not change an id,
  - migration strategy,
  - version compatibility rules.

Expected docs:

```txt
Tools/SystemDefinitionEngine/docs/SDE_ID_STABILITY.md
```

---

## 13. Artifact Emission

- [ ] Add artifact manifest output.

  Done means SDE emits a manifest containing:

  - artifact id,
  - artifact kind,
  - source files,
  - schema version,
  - content hash,
  - dependencies,
  - diagnostics summary,
  - generated output paths.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Artifacts/ArtifactManifest.hpp
Tools/SystemDefinitionEngine/include/SDE/Artifacts/ArtifactEmitter.hpp
Tools/SystemDefinitionEngine/src/Artifacts/ArtifactEmitter.cpp
```

- [ ] Emit runtime-consumable artifacts.

  Done means artifacts can be consumed by runtime/server/editor integration without requiring SDE internals.

- [ ] Emit diagnostics artifact.

  Done means diagnostics can be consumed by editor Problems panel, Forge, CI, or Saga product workflows.

- [ ] Keep artifact formats versioned.

  Done means every emitted artifact includes:

  - format version,
  - compiler version,
  - schema version,
  - compatibility metadata.

---

## 14. Code Generation

- [ ] Add optional code generation backend.

  Done means SDE can generate code artifacts when explicitly requested.

Possible generated outputs:

```txt
component registration code
schema reflection data
serialization descriptors
runtime binding descriptors
editor metadata descriptors
```

- [ ] Keep generated code deterministic.

  Done means generated code:

  - has stable ordering,
  - avoids timestamps unless explicitly disabled or separated,
  - uses stable names,
  - includes generated-file warning,
  - includes compiler/schema version.

- [ ] Keep codegen backend independent from engine/editor headers.

  Done means generated code may target Saga runtime conventions, but SDE compiler implementation does not include Saga runtime/editor headers.

Important distinction:

```txt
SDE may generate code for Saga.
SDE must not include Saga to generate that code.
```

This distinction apparently saves entire projects from eating themselves.

---

## 15. Diagnostics

- [ ] Add structured diagnostic system.

  Done means every diagnostic includes:

  - severity,
  - code,
  - message,
  - source file,
  - source range,
  - related notes,
  - recoverability,
  - phase.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Diagnostics/Diagnostic.hpp
Tools/SystemDefinitionEngine/include/SDE/Diagnostics/DiagnosticCode.hpp
Tools/SystemDefinitionEngine/include/SDE/Diagnostics/DiagnosticSeverity.hpp
Tools/SystemDefinitionEngine/include/SDE/Diagnostics/DiagnosticSink.hpp
Tools/SystemDefinitionEngine/src/Diagnostics/Diagnostic.cpp
```

- [ ] Support human-readable diagnostics.

  Done means CLI output shows:

  - file path,
  - line,
  - column,
  - highlighted range where possible,
  - readable explanation,
  - suggested fix where safe.

- [ ] Support JSON diagnostics.

  Done means tools and CI can consume diagnostics in a stable machine-readable format.

- [ ] Add diagnostic tests.

  Required coverage:

  - syntax error location,
  - semantic error location,
  - duplicate symbol notes,
  - dependency cycle notes,
  - JSON diagnostic schema stability.

Diagnostics that say only “invalid definition” should be considered an insult, not a feature.

---

## 16. Incremental Compilation

- [ ] Add dependency graph tracking.

  Done means SDE tracks:

  - source file dependencies,
  - import dependencies,
  - schema dependencies,
  - artifact dependencies,
  - generated output dependencies.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Incremental/DependencyGraph.hpp
Tools/SystemDefinitionEngine/include/SDE/Incremental/CompileCache.hpp
Tools/SystemDefinitionEngine/src/Incremental/DependencyGraph.cpp
```

- [ ] Add compile cache.

  Done means unchanged inputs can reuse previous results safely.

- [ ] Add invalidation logic.

  Done means changes invalidate:

  - affected source file,
  - dependent schemas,
  - dependent artifacts,
  - generated code outputs.

- [ ] Add incremental compile tests.

  Required coverage:

  - no-change compile reuses cache,
  - changed file invalidates dependents,
  - deleted import invalidates dependents,
  - schema version change invalidates artifacts,
  - cache corruption is detected.

---

## 17. Formatting

- [ ] Add `sde format`.

  Done means SDE can format source files with:

  - stable indentation,
  - stable declaration ordering where appropriate,
  - stable attribute formatting,
  - preserved comments where supported,
  - check-only mode for CI.

- [ ] Add formatting tests.

  Required coverage:

  - simple file,
  - nested declarations,
  - comments,
  - attributes,
  - invalid file handling,
  - idempotent formatting.

Formatting should be idempotent.

If running the formatter twice changes the file twice, congratulations, you built a slot machine.

---

## 18. Validation Rules

- [ ] Add schema-level validation.

  Done means SDE validates:

  - schema version,
  - required declarations,
  - duplicate names,
  - invalid type references,
  - invalid dependency declarations,
  - invalid metadata.

- [ ] Add graph validation.

  Done means SDE validates:

  - node existence,
  - edge existence,
  - compatible edge endpoints,
  - cycles where forbidden,
  - required inputs,
  - output type compatibility.

- [ ] Add package/resource validation.

  Done means SDE validates:

  - resource references,
  - package references,
  - missing artifacts,
  - invalid artifact kinds,
  - incompatible package versions.

---

## 19. Runtime Integration Outputs

- [ ] Emit runtime schema artifacts.

  Done means SagaEngine runtime can consume:

  - component schema ids,
  - serialization descriptors,
  - runtime graph artifacts,
  - resource binding descriptors,
  - validation metadata.

- [ ] Emit server validation artifacts.

  Done means SagaServer can consume:

  - authority-relevant schema data,
  - message descriptors,
  - event descriptors,
  - validation rules,
  - deterministic hashes.

- [ ] Keep runtime/server integration output-only.

  Done means runtime/server do not include SDE compiler internals.

Correct flow:

```txt
SDE emits artifacts.
Runtime/server consume artifacts.
```

Incorrect flow:

```txt
Runtime/server link SDE compiler internals.
```

---

## 20. Editor Integration Outputs

- [ ] Emit editor metadata artifacts.

  Done means SagaEditor can consume:

  - display names,
  - categories,
  - field metadata,
  - graph node metadata,
  - validation diagnostics,
  - source location mappings.

- [ ] Support editor Problems panel integration.

  Done means SDE diagnostics can be shown in editor without editor including SDE parser/AST internals.

- [ ] Support editor graph tooling through artifacts.

  Done means editor graph surfaces consume stable metadata/artifacts rather than compiler internals.

---

## 21. Forge Integration

- [ ] Allow Forge to invoke SDE as a compiler step.

  Done means Forge can:

  - run SDE validate,
  - run SDE compile,
  - collect SDE diagnostics,
  - consume artifact manifests,
  - fail builds when SDE compilation fails.

- [ ] Keep Forge as build frontend, not SDE owner.

  Done means Forge does not own:

  - SDE parser,
  - SDE AST,
  - SDE IR,
  - SDE semantic rules,
  - SDE codegen.

Forge can run SDE.

Forge does not become SDE.

Tiny distinction. Massive consequences.

---

## 22. Prism Integration

- [ ] Allow Prism to consume SDE outputs for code intelligence.

  Done means Prism can use:

  - source maps,
  - symbol information,
  - dependency graphs,
  - artifact manifests,
  - schema metadata.

- [ ] Keep Prism as code intelligence, not compiler truth.

  Done means Prism does not own:

  - SDE parsing truth,
  - SDE semantic validation truth,
  - SDE artifact emission,
  - SDE hash/id policy.

Prism may index and analyze.

SDE compiles.

Nobody gets to steal the other tool’s job and call it “integration”.

---

## 23. SagaTools Integration

- [ ] Allow SagaTools to dispatch SDE commands.

  Example commands:

  ```txt
  sagatools sde validate <schema>
  sagatools sde compile <schema> --out <dir>
  sagatools sde inspect <artifact>
  sagatools sde doctor
  ```

- [ ] Keep SagaTools as dispatcher only.

  Done means SagaTools does not include:

  - SDE parser,
  - SDE AST,
  - SDE semantic analyzer,
  - SDE codegen,
  - SDE compile cache.

---

## 24. Build and CI Integration

- [ ] Add SDE compile step to build workflows where needed.

  Done means build can fail on:

  - syntax errors,
  - semantic errors,
  - invalid graph definitions,
  - artifact generation failure,
  - deterministic hash mismatch.

- [ ] Add CI validation command.

  Required command:

  ```txt
  sde validate --workspace <path>
  ```

- [ ] Add CI compile command.

  Required command:

  ```txt
  sde compile --workspace <path> --out <dir>
  ```

- [ ] Add deterministic output check.

  Done means CI can verify that repeated compile produces identical output.

---

## 25. Configuration

- [ ] Add SDE configuration file support.

  Done means config can define:

  - source roots,
  - include paths,
  - output path,
  - artifact format,
  - enabled backends,
  - warning levels,
  - strictness mode,
  - cache path.

Expected files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Config/SdeConfig.hpp
Tools/SystemDefinitionEngine/include/SDE/Config/ConfigLoader.hpp
Tools/SystemDefinitionEngine/src/Config/ConfigLoader.cpp
```

- [ ] Add command-line config override.

  Done means CLI flags can override config fields in a documented and deterministic way.

- [ ] Add config validation.

  Done means invalid config fails before compile begins.

---

## 26. Artifact Formats

- [ ] Define JSON artifact output for inspection and tooling.

  Done means JSON output is:

  - versioned,
  - deterministic,
  - schema-documented,
  - stable enough for tools.

- [ ] Define binary artifact output for runtime where needed.

  Done means binary output is:

  - versioned,
  - endian-aware where required,
  - deterministic,
  - validated on load,
  - documented.

- [ ] Add artifact compatibility policy.

  Done means docs explain:

  - compatible changes,
  - incompatible changes,
  - migration rules,
  - version rejection behavior.

Expected docs:

```txt
Tools/SystemDefinitionEngine/docs/SDE_ARTIFACT_FORMATS.md
```

---

## 27. Error and Exit Code Policy

- [ ] Define stable SDE exit codes.

  Required categories:

  ```txt
  0   success
  1   general failure
  2   invalid arguments
  3   config error
  4   parse error
  5   semantic error
  6   validation error
  7   artifact emission error
  8   dependency cycle
  9   cache error
  10  internal compiler error
  ```

- [ ] Keep internal compiler errors distinct.

  Done means compiler bugs are reported separately from user-authored schema errors.

There is a difference between “your file is wrong” and “the compiler tripped over its own shoelaces”.

The user deserves to know which disaster occurred.

---

## 28. Logging

- [ ] Add compiler logging.

  Done means logs can include:

  - compile phase,
  - file count,
  - dependency count,
  - cache hit/miss,
  - artifact count,
  - elapsed time,
  - diagnostics count.

- [ ] Keep normal CLI output readable.

  Done means normal output avoids dumping internal compiler noise unless `--verbose` is enabled.

- [ ] Support machine-readable logging where needed.

  Done means CI can consume compile summary in JSON mode.

---

## 29. Performance

- [ ] Define performance targets.

  Done means SDE tracks:

  - parse time,
  - semantic analysis time,
  - IR generation time,
  - artifact emission time,
  - cache load/save time,
  - total compile time.

- [ ] Add performance diagnostics.

  Done means `--verbose` or `--profile` can show phase timings.

- [ ] Add large-project compile test.

  Done means SDE can compile a representative large schema set within acceptable time and memory bounds.

---

## 30. Testing Roadmap

### 30.1 Unit Tests

- [ ] Add lexer tests.

- [ ] Add parser tests.

- [ ] Add AST tests.

- [ ] Add semantic validator tests.

- [ ] Add IR builder tests.

- [ ] Add hashing tests.

- [ ] Add artifact emitter tests.

- [ ] Add diagnostic formatting tests.

- [ ] Add config loader tests.

---

### 30.2 Snapshot Tests

- [ ] Add AST snapshot tests.

- [ ] Add IR snapshot tests.

- [ ] Add artifact manifest snapshot tests.

- [ ] Add diagnostic JSON snapshot tests.

Generated outputs must be stable.

Otherwise “snapshot test” becomes “today’s random nonsense with brackets”.

---

### 30.3 Integration Tests

- [ ] Add full compile integration test.

  Done means:

  - input source parses,
  - semantics pass,
  - IR emits,
  - artifacts emit,
  - manifest emits,
  - diagnostics are empty or expected.

- [ ] Add invalid project integration test.

  Done means invalid input produces stable diagnostics and non-zero exit code.

- [ ] Add incremental compile integration test.

- [ ] Add generated code integration test where codegen exists.

---

### 30.4 Determinism Tests

- [ ] Add repeated compile determinism test.

- [ ] Add shuffled input order determinism test.

- [ ] Add clean directory determinism test.

- [ ] Add cache/no-cache equivalence test.

- [ ] Add generated output hash test.

---

## 31. CI Requirements

- [ ] Add SDE unit tests to CI.

- [ ] Add SDE integration tests to CI.

- [ ] Add SDE deterministic output tests to CI.

- [ ] Add SDE dependency boundary checks to CI.

Required forbidden dependency checks:

```txt
Tools/SystemDefinitionEngine/** must not include Saga/**
Tools/SystemDefinitionEngine/** must not include Engine/**
Tools/SystemDefinitionEngine/** must not include Editor/**
Tools/SystemDefinitionEngine/** must not include Server/**
Tools/SystemDefinitionEngine/** must not include Shared/**
Tools/SystemDefinitionEngine/** must not include Collaboration/**
Tools/SystemDefinitionEngine/** must not include Tools/Forge/**
Tools/SystemDefinitionEngine/** must not include Tools/Prism/**
Tools/SystemDefinitionEngine/** must not include Tools/SagaTools/**
```

- [ ] Add artifact compatibility check.

  Done means incompatible artifact format changes fail or require explicit version bump.

---

## 32. Recommended File Layout

Recommended target layout:

```txt
Tools/SystemDefinitionEngine/
  SDE_ROADMAP.md
  README.md
  CMakeLists.txt or Cargo.toml

Tools/SystemDefinitionEngine/docs/
  SDE_LANGUAGE.md
  SDE_ARTIFACT_FORMATS.md
  SDE_ID_STABILITY.md
  SDE_DIAGNOSTICS.md

Tools/SystemDefinitionEngine/include/SDE/
  Compiler.hpp
  CompileRequest.hpp
  CompileResult.hpp

Tools/SystemDefinitionEngine/include/SDE/Lexer/
  Token.hpp
  Lexer.hpp

Tools/SystemDefinitionEngine/include/SDE/Parser/
  Parser.hpp
  ParseResult.hpp

Tools/SystemDefinitionEngine/include/SDE/AST/
  AstNode.hpp
  AstFile.hpp
  AstDeclaration.hpp
  AstTypeRef.hpp

Tools/SystemDefinitionEngine/include/SDE/Semantic/
  Symbol.hpp
  SymbolTable.hpp
  SemanticValidator.hpp

Tools/SystemDefinitionEngine/include/SDE/IR/
  IrModule.hpp
  IrNode.hpp
  IrType.hpp
  IrGraph.hpp

Tools/SystemDefinitionEngine/include/SDE/Artifacts/
  ArtifactManifest.hpp
  ArtifactEmitter.hpp

Tools/SystemDefinitionEngine/include/SDE/Diagnostics/
  Diagnostic.hpp
  DiagnosticCode.hpp
  DiagnosticSeverity.hpp
  DiagnosticSink.hpp

Tools/SystemDefinitionEngine/include/SDE/Incremental/
  DependencyGraph.hpp
  CompileCache.hpp

Tools/SystemDefinitionEngine/src/
  main.cpp or main.rs
  Compiler.cpp
  Lexer/
  Parser/
  AST/
  Semantic/
  IR/
  Artifacts/
  Diagnostics/
  Incremental/
  Config/
  Hash/

Tools/SystemDefinitionEngine/tests/
  LexerTests.cpp
  ParserTests.cpp
  SemanticTests.cpp
  IrTests.cpp
  ArtifactTests.cpp
  DeterminismTests.cpp
  IntegrationTests.cpp
```

This layout is illustrative.

The ownership rule is not illustrative.

SDE owns SDE.

---

## 33. Migration Plan

- [ ] Remove any dependency from SDE to Saga modules.

- [ ] Move SDE-specific logic out of SagaTools if it exists there.

- [ ] Move build workflow behavior out of SDE and into Forge where appropriate.

- [ ] Move code intelligence behavior out of SDE and into Prism where appropriate.

- [ ] Keep artifact contracts stable and documented.

- [ ] Add dependency boundary checks.

- [ ] Add deterministic compile tests before relying on SDE artifacts in runtime/editor workflows.

---

## 34. Non-Goals

SDE does not own:

- Saga product shell,
- editor graph panel UI,
- editor Problems panel UI,
- runtime ECS implementation,
- server authority,
- collaboration sessions,
- build frontend UX,
- code intelligence database,
- tool dispatch,
- asset importer UI,
- package publishing service.

Related ownership:

| Area | Owner |
|---|---|
| Product shell | `Saga` |
| Editor UI | `SagaEditor` |
| Runtime/server consumption | `SagaEngine` / `SagaServer` |
| Shared artifact references | `SagaShared` |
| Collaboration implementation | `SagaCollaboration` |
| Build frontend | `Forge` |
| Code intelligence | `Prism` |
| Tool dispatch | `SagaTools` |

---

## 35. Production Definition of Done

- [ ] SDE is standalone and does not depend on Saga modules.

- [ ] SDE CLI supports validate, compile, inspect, format, and doctor.

- [ ] Source language is documented.

- [ ] Lexer and parser are tested.

- [ ] AST is compiler-internal.

- [ ] Semantic analysis catches invalid definitions.

- [ ] Canonical IR is deterministic.

- [ ] Stable hashes and ids are generated.

- [ ] Artifact manifests are emitted.

- [ ] Runtime/editor/server/tools consume outputs without importing compiler internals.

- [ ] Diagnostics are structured, readable, and machine-readable.

- [ ] Incremental compilation is safe and deterministic.

- [ ] Formatting is idempotent.

- [ ] Artifact formats are versioned.

- [ ] Exit codes are stable.

- [ ] CI verifies tests, determinism, and dependency boundaries.

---

## 36. Final Architecture Rule

SDE should remain:

```txt
standalone,
deterministic,
boring in output,
strict in validation,
clear in diagnostics,
and impossible to confuse with runtime/editor/product ownership.
```

It should know:

```txt
how to parse definitions,
how to validate them,
how to lower them into canonical IR,
how to emit artifacts,
how to explain failure.
```

It should not know:

```txt
how the editor draws panels,
how the runtime simulates entities,
how the server owns authority,
how Saga opens projects,
how Forge builds packages,
how Prism indexes code,
or how SagaTools dispatches commands.
```

A compiler that keeps its boundaries is useful.

A compiler that imports the entire engine is just a hostage situation with better type names.