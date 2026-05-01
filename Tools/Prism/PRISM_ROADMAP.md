# Prism — Roadmap

> Last updated: 2026-04-29
> Primary home: `Tools/Prism/` inside the SagaEngine repository.
> Future: may graduate to a standalone repository when the tool
> stabilises and is useful outside SagaEngine's own codebase.

Prism is the codebase indexer and AI memory graph builder for SagaEngine.
It consists of two independent layers that communicate through a versioned
JSON schema: `prism-extract` (C++ / Clang LibTooling) and `prism-graph`
(Python pipeline). Every item below belongs to one or both layers.

Conventions: `[x]` shipped and committed, `[ ]` open.
When an item ships, the note after it names the files that represent the work.

---

## 1. Extraction Core (C++ / Clang LibTooling)

The extractor is the only component that has direct access to the Clang AST.
Everything it emits becomes a permanent part of the versioned raw JSON schema,
so correctness here is more important than completeness.

| Status | Item |
|--------|------|
| [x] | `RecursiveASTVisitor` with unified `VisitNamedDecl` dispatch — single entry point for all declaration kinds. → `AST/SymbolVisitor` |
| [x] | `DeclExtractor` — all Clang API calls isolated in one class, kind-specific logic contained. → `AST/DeclExtractor` |
| [x] | `RelationshipCollector` — base class spellings and type-level dependency USRs extracted separately from scalar fields. → `AST/RelationshipCollector` |
| [x] | Template instantiation indexing — `shouldVisitTemplateInstantiations()` enabled; implicit specialisations are included. → `AST/SymbolVisitor` |
| [x] | Stable symbol IDs — 16-char SHA-1 hex derived from Clang USR; fallback for anonymous and macro-generated declarations. → `Support/IdGen` |
| [x] | Brief-comment extraction — Doxygen `@brief` and bare leading-line styles both supported. → `Support/CommentParser` |
| [x] | Repository-relative POSIX path normalisation — all paths stored relative to `--repo-root`. → `Support/PathUtils` |
| [x] | System-header guard — declarations from system paths are never indexed. → `Support/PathUtils`, `AST/DeclExtractor` |
| [x] | Streaming JSON emitter — writes directly to `ofstream`; no intermediate in-memory representation. → `Emit/JsonWriter` |
| [x] | `ResultAccumulator` with mutex — thread-safe aggregation of per-TU results for future parallel execution. → `Pipeline/PrismAction` |
| [x] | Partial-result emission — JSON is written even when some translation units have parse errors. → `main.cpp` |
| [x] | `--repo-root` and `-o` CLI options via `CommonOptionsParser`. → `main.cpp` |
| [ ] | Incremental extraction — skip translation units whose source files and compile flags have not changed since the last run. Requires a persistent state file keyed on file path + mtime + flag hash. |
| [ ] | Parallel TU parsing — run multiple `ClangTool` workers in a thread pool; each worker owns its own `ASTUnit`. Blocked by `ResultAccumulator` thread-safety (already in place). |
| [ ] | Compile database fallback — when no `compile_commands.json` exists, synthesise conservative arguments from `--include-dir` flags rather than aborting. |
| [ ] | Macro expansion tracking — record which macros expand at each declaration site; useful for engine-specific annotation macros (`SAGA_COMPONENT`, `SAGA_SYSTEM`, …). |
| [ ] | Cross-TU call graph — actual call edges (caller → callee USR) extracted from function bodies using a `RecursiveASTVisitor` in body-traversal mode, kept behind a `--call-graph` flag because it is expensive. |
| [ ] | Virtual dispatch resolution — map virtual method declarations to their concrete overriders using `CXXMethodDecl::getOverriddenMethods()`; emit as a separate `overrides` list on the symbol record. |
| [ ] | C++20 concept and requires-clause extraction — emit `requires` expressions as a `constraints` field on template declarations. |
| [ ] | Anonymous namespace handling — declarations inside anonymous namespaces receive a synthetic qualified name based on their TU path to avoid USR collisions across TUs. |
| [ ] | Diagnostic consumer — capture Clang parse diagnostics (errors, warnings, notes) into the raw JSON `diagnostics` array with file, line, and severity. |
| [ ] | `--filter-scope` flag — limit extraction to a list of repository-relative directory prefixes; avoids parsing `ThirdParty/` and `Tests/` when not needed. |
| [ ] | Windows / MSVC LLVM validation — build and test `prism-extract` against the LLVM distribution bundled with Visual Studio and document any required workarounds. |
| [ ] | macOS / Apple Silicon validation — verify the `find_package(LLVM)` path works with Homebrew LLVM on arm64 and document the required `-DLLVM_DIR` value. |

---

## 2. Raw JSON Schema

`Record.h` and the emitted JSON are the only contract between the C++ and
Python layers. Changes here require a schema-version bump and a migration note.

| Status | Item |
|--------|------|
| [x] | `SymbolRecord` — id, usr, name, qualified\_name, display\_name, kind, access, is\_definition, brief, raw\_comment, signature, result\_type, type\_spelling, location, extent, bases, deps, children. → `Record.h` |
| [x] | `FileRecord` — path, brief, includes, symbol\_ids. → `Record.h` |
| [x] | `ExtractionResult` — flat lists of files and symbols; no index structures at this layer. → `Record.h` |
| [x] | Schema version field `"schema_version": "1.0"` in emitted JSON. → `Emit/JsonWriter` |
| [ ] | `diagnostics` array — Clang parse errors and warnings per file. Blocked on Diagnostic consumer item above. |
| [ ] | `generated_flags` field — the actual compile flags used per TU, stored for reproducibility and debugging. |
| [ ] | Binary output mode (`--format=msgpack`) — for repositories with over 100 k symbols the JSON parse time becomes significant; MessagePack reduces it by roughly 4×. |
| [ ] | Schema migration utility — a Python script that upgrades a `1.0` raw JSON file to `1.1` without re-running the extractor. |

---

## 3. Graph Builder (Python Pipeline)

The builder transforms the raw extraction into a dependency-resolved,
module-aggregated AI memory graph. It must be deterministic: the same raw
JSON always produces the same graph.

| Status | Item |
|--------|------|
| [x] | Five-phase pipeline: deduplication → USR index → dependency resolution → file and module aggregation → statistics. → `graph/builder.py` |
| [x] | Symbol deduplication by stable ID — definition site wins for documentation fields; relationship lists are union-merged. → `graph/builder.py` |
| [x] | USR → stable-ID resolution — cross-file dependencies stored as USRs are resolved to graph IDs after all TUs are processed. → `graph/builder.py` |
| [x] | Self-reference guard — a symbol's own ID is never added to its `deps` list. → `graph/builder.py` |
| [x] | File category assignment — headers, sources, cmake, others. → `graph/classifier.py` |
| [x] | Two-level module derivation from POSIX path — `Engine/Renderer/X.h` → `Engine/Renderer`. → `graph/classifier.py` |
| [x] | Disk-based line count — read from disk using `repo_root` from the raw JSON. → `graph/builder.py` |
| [x] | `RawExtraction` defensive loader — missing or wrong-typed fields fall back to empty values rather than raising. → `graph/loader.py` |
| [ ] | Dependency cycle detection — detect and report circular dependency chains in the resolved graph; emit as a `cycles` list in `stats`. |
| [ ] | Reverse-dep index — for each symbol, compute `used_by` (callers / field owners) from the forward `deps` graph; useful for impact analysis. |
| [ ] | Orphan symbol detection — symbols referenced in `deps` or `children` but not present in the graph are flagged in `stats.orphans`. |
| [ ] | Module boundary violation detection — a source file in `Engine/` including a header from `Editor/` is flagged as a boundary crossing. |
| [ ] | Graph query API (`prism/query.py`) — `find_subclasses(id)`, `find_callers(id)`, `find_by_kind(kind)`, `find_by_module(module)` operating on an in-memory `GraphData`. |
| [ ] | Watch mode / daemon — `prism-graph --watch` monitors `prism.raw.json` for changes via `inotify` / `FSEvents` and rebuilds incrementally on modification. |
| [ ] | Caching layer — serialise the resolved `GraphData` to a binary cache keyed on `raw_json` mtime; skip the five-phase build on unchanged input. |
| [ ] | Configurable symbol filter — `--exclude-pattern` and `--include-pattern` glob flags to drop or keep specific qualified names from the final graph. |

---

## 4. Output Formats

Both outputs are produced by concrete `Exporter` subclasses that implement
a single `write(graph, out_path)` method. Adding a format means one new file.

| Status | Item |
|--------|------|
| [x] | `JsonExporter` — deterministically-ordered, indented UTF-8 JSON; field names are the public AI contract. → `export/json_exporter.py` |
| [x] | `TxtExporter` — 80-column human-readable summary with double-rule section separators, per-file symbol groups, and truncated dep lists. → `export/txt_exporter.py` |
| [x] | Abstract `Exporter` base — single `write()` method; new formats are one subclass. → `export/base.py` |
| [ ] | `MarkdownExporter` — module-per-section markdown document suitable for pasting into a wiki or a `CONTEXT.md` at repository root. |
| [ ] | `DotExporter` — Graphviz `.dot` file of the symbol dependency graph; scoped to a single module or a depth limit to remain readable. |
| [ ] | `CSVExporter` — flat symbol table for spreadsheet analysis; one row per symbol with all scalar fields. |
| [ ] | Streaming JSON mode — emit the graph as a newline-delimited JSON stream (one object per line) for large codebases that exceed editor context windows. |

---

## 5. CLI and Configuration

Both binaries expose flat, composable CLI interfaces.
No configuration file is required for a basic run.

| Status | Item |
|--------|------|
| [x] | `prism-extract --repo-root <path> -o <file> -p <build-dir>` — full ClangTool option set including extra-arg passthrough. → `main.cpp` |
| [x] | `prism-graph [raw_json] --out-dir --json-name --txt-name --no-txt -v -q` → `run.py` |
| [x] | `PipelineConfig` — frozen dataclass, single source of all defaults, validated on construction. → `prism/config.py` |
| [x] | `PrismError` exception hierarchy — `RawJsonNotFoundError`, `RawJsonSchemaError`, `ExportError`, `ConfigError`. → `prism/errors.py` |
| [x] | Structured logger with live progress bar — stderr for diagnostics, stdout for results. → `prism/log.py` |
| [ ] | `prism` wrapper script — single entry point that sequences `prism-extract` then `prism-graph` with shared `--repo-root` and `--out-dir`; no manual two-step. |
| [ ] | `--config <file>` support — optional TOML configuration file for persistent settings (output paths, filter patterns, worker count) so CI invocations stay short. |
| [ ] | `--scope Engine Editor` flag on `prism-extract` — restrict extraction to a set of repository sub-directories and skip everything else including `Tests/` and `ThirdParty/`. |
| [ ] | `--dry-run` mode — report which files would be parsed and what the output paths would be without writing anything. |
| [ ] | Exit code contract — `0` success, `1` configuration or I/O error, `2` partial parse failure (some TUs errored but output was still written). |
| [ ] | Shell completion — `bash` and `zsh` completion scripts for both binaries generated from the argparse definition. |

---

## 6. Build System and Distribution

| Status | Item |
|--------|------|
| [x] | Root `CMakeLists.txt` with `find_package(LLVM)` and `find_package(Clang)` via CMake config exports. → `CMakeLists.txt` |
| [x] | `extractor/CMakeLists.txt` — explicit source list, full Clang link set, `-Wall -Wextra -Wpedantic`, install rule. → `extractor/CMakeLists.txt` |
| [x] | `pipeline/pyproject.toml` — `prism-pipeline` package, `prism-graph` entry point, no runtime dependencies. → `pipeline/pyproject.toml` |
| [ ] | CI workflow (GitHub Actions / internal CI) — matrix build across Linux (LLVM 17, 18) and macOS (Homebrew LLVM); run extractor on a fixture directory and diff the output against a golden file. |
| [ ] | Golden-file test fixture — a small self-contained C++ directory under `Tests/Prism/fixture/` with a hand-authored `expected.graph.json`; checked in and updated deliberately. |
| [ ] | Python test suite (`pytest`) — unit tests for `loader.py`, `builder.py`, `classifier.py`, and each exporter against synthetic `RawExtraction` inputs. |
| [ ] | `cmake --preset release` preset — `CMAKE_BUILD_TYPE=Release`, LTO enabled, strip on install. |
| [ ] | Homebrew formula (internal tap) — install `prism-extract` on macOS developer machines with a single `brew install` command. |
| [ ] | Pre-built binary distribution — attach signed `prism-extract` binaries for Linux x86\_64, Linux arm64, and macOS arm64 to tagged releases so Python-only users never need to build from source. |
| [ ] | `pip install prism-pipeline` from internal PyPI index — separates the Python tool from the C++ build for teams that only need the graph builder. |

---

## 7. Engine Integration

Items that are specific to running Prism inside the SagaEngine repository.
If Prism becomes a standalone tool these items move to a separate integration
guide rather than disappearing from scope.

| Status | Item |
|--------|------|
| [ ] | `SagaEngineRoot.marker` presence check in `prism-graph` — the Python side currently trusts `repo_root` from the raw JSON without verifying the marker. Add an optional `--verify-root` flag. |
| [ ] | Engine-aware scope presets — `--scope engine`, `--scope editor`, `--scope server` as named aliases for the actual SagaEngine directory structure rather than requiring full path strings. |
| [ ] | Macro annotation awareness — `SAGA_COMPONENT`, `SAGA_SYSTEM`, `SAGA_PROPERTY`, and similar engine macros are recognised and their semantic meaning is propagated to the symbol record as a `tags` list. |
| [ ] | Asset reference extraction — detect string literals and resource path constants that reference engine asset paths; emit as a `asset_refs` list on the enclosing symbol. |
| [ ] | ECS-layer graph enrichment — components, systems, and queries are grouped into an `ecs` section in the graph that mirrors the engine's archetype layout. |
| [ ] | CI gate — Prism runs on every PR that touches `Engine/`, `Editor/`, or `Server/`; the output diff is posted as a PR comment summarising added, removed, and changed symbols. |
| [ ] | `CONTEXT.md` auto-generation — after each successful Prism run, regenerate `CONTEXT.md` at repository root from the graph summary; committed by the CI bot. |

---

## 8. Schema Stability and Versioning

The raw JSON schema (`Record.h` → `prism.raw.json`) and the graph schema
(`GraphData` → `prism.graph.json`) are independent versioned contracts.
Breaking either requires a coordinated bump.

| Status | Item |
|--------|------|
| [x] | `schema_version: "1.0"` field in both output files. |
| [ ] | Formal schema changelog — `SCHEMA.md` documents every field, its type, its invariants, and the version it was introduced in. |
| [ ] | Schema validation utility — `prism-graph --validate-only` checks a raw JSON against the current schema and reports field-level errors without building the graph. |
| [ ] | Forward compatibility policy — the loader accepts unknown fields silently; only missing required fields are errors. Currently implemented defensively but not formally documented. |
| [ ] | Automated schema diff on CI — diff the emitted schema against the previous tagged version and block merge if breaking changes are undocumented. |

---

## 9. Future Scope (Post-1.0)

Items that are clearly valuable but intentionally deferred until the core
is stable and the SagaEngine integration is proven in daily use.

| Status | Item |
|--------|------|
| [ ] | Language server protocol (LSP) bridge — expose the Prism graph as an LSP workspace symbol index so editors can query it without reading the JSON. |
| [ ] | Standalone repository — separate `prism` git repository with its own versioning, CI, and issue tracker; SagaEngine pins a specific release tag. |
| [ ] | Support for additional languages — Rust (`syn`-based extractor), C# (`Roslyn`-based extractor) feeding the same graph schema. |
| [ ] | Interactive graph explorer — a local web UI (`http://localhost:PORT`) that renders the module dependency graph, lets the user drill into symbols, and exports sub-graphs for pasting into AI prompts. |
| [ ] | AI prompt assembler — given a set of symbol IDs or a query, produce a compact, context-window-efficient prompt fragment from the graph that includes only the directly relevant declarations and their one-hop neighbours. |
| [ ] | Prism SDK — a documented extension API that lets engine-specific tools register custom extractors, custom graph enrichers, and custom exporters without forking the core. |

---

## Definitions

- **Shipped**: implemented, code-reviewed, and present in the current
  repository state. Marked `[x]`.
- **Open**: planned and scoped but not yet implemented. Marked `[ ]`.
- **Post-1.0**: explicitly deferred; will not block a 1.0 tag.

When an item ships, replace `[ ]` with `[x]` and append a note naming the
files that represent the work, matching the convention used in the SagaEngine
roadmap documents.
