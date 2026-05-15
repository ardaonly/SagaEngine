# Prism

> **C++ codebase → AI memory graph.**  
> Real compiler frontend. Structured output. No regex.

---

## What it does

Prism parses every C++ translation unit in a repository using the Clang
compiler frontend, extracts declarations with their documentation, type
relationships, and include graph, then builds a dependency-resolved symbol
graph that AI assistants can navigate directly.

The output is a JSON graph file - not a raw code dump - structured for
efficient AI consumption with no context wasted on boilerplate.

Prism is Apache-2.0 under Arda Koyuncu. It is designed as a standalone tool for
any C++ codebase that can provide a Clang-compatible compilation database.
When distributed from a larger monorepo, that repository is only the current
development host; Prism's public interface remains the command-line tools and
the versioned JSON schemas documented below.

---

## Architecture

```
prism/
│
├── extractor/                    C++ binary: prism-extract
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp              ClangTool entry - options, run, emit
│       ├── Record.h              POD structs - the only contract between layers
│       │
│       ├── Support/
│       │   ├── PathUtils         Path relativization and system-header detection
│       │   ├── IdGen             Stable 16-char SHA-1 IDs from Clang USR strings
│       │   └── CommentParser     Brief extraction from Doxygen / triple-slash blocks
│       │
│       ├── AST/
│       │   ├── DeclExtractor     NamedDecl -> SymbolRecord (all Clang API calls here)
│       │   ├── RelationshipCollector   Bases and type-level dependency USRs
│       │   └── SymbolVisitor     RecursiveASTVisitor — unified VisitNamedDecl dispatch
│       │
│       ├── Pipeline/
│       │   ├── PrismConsumer     ASTConsumer — drives SymbolVisitor per TU
│       │   └── PrismAction       FrontendAction + FrontendActionFactory
│       │
│       └── Emit/
│           └── JsonWriter        Streaming JSON serializer (no third-party libs)
│
└── pipeline/                     Python package: prism-graph
    ├── run.py                    CLI entry point
    ├── pyproject.toml
    └── prism/
        ├── config.py             PipelineConfig — frozen dataclass, single defaults source
        ├── errors.py             Exception hierarchy (PrismError → specific types)
        ├── log.py                Structured logger + live progress bar
        ├── schema.py             Raw* types + SymbolNode / FileNode / GraphData
        ├── graph/
        │   ├── loader.py         Defensive JSON -> RawExtraction deserialization
        │   ├── builder.py        Five-phase graph construction (dedup -> resolve -> aggregate)
        │   └── classifier.py     File category assignment and module name derivation
        └── export/
            ├── base.py           Abstract Exporter interface
            ├── json_exporter.py  -> prism.graph.json (public AI contract)
            └── txt_exporter.py   -> prism.txt (human-readable summary)
```

---

## Data Flow

```
C++ source files
       │
       ▼  Clang LibTooling - real AST, real type system
prism-extract
       │
       ▼  stable JSON - no Clang types cross this boundary
prism.raw.json
       │
       ▼  Python - dedup, USR resolution, module aggregation
prism-graph
       │
       ├──>  prism.graph.json    machine-readable AI memory graph
       └──>  prism.txt           human-readable summary
```

---

## Installation

### Requirements

| Component     | Minimum version |
|---------------|-----------------|
| CMake         | 3.22            |
| C++ compiler  | C++17           |
| LLVM / Clang  | 16              |
| Python        | 3.11            |

Prism provides a unified bootstrap script that automates all installation steps:

```bash
# Full installation (C++ extractor + Python pipeline)
python3 Tools/Prism/build.py

# Python-only install (no LLVM required)
python3 Tools/Prism/build.py --skip-extractor

# Debug build with clean rebuild
python3 Tools/Prism/build.py --debug --clean

# Run lightweight wrapper tests after staging
python3 Tools/Prism/build.py --run-tests

# Explicit LLVM path if auto-detection fails
python3 Tools/Prism/build.py --llvm-dir /usr/lib/llvm-18/lib/cmake/llvm
```

After installation, binaries are staged to `Tools/Prism/bin/`:
- `prism`: Unified launcher for extraction and graph generation
- `prism-extract`: C++ extractor (requires LLVM; skipped if missing)
- `prism-graph`: Python pipeline launcher (always available)

---

## Usage

### Minimal Example First

`Tools/Prism/examples/minimal/` contains one small `.h` file and one small
`.cpp` file. Use it to verify Prism before pointing the tool at a production
codebase.

First, generate the example compile database with CMake:

```bash
cmake \
    -S Tools/Prism/examples/minimal \
    -B Tools/Prism/examples/minimal/build \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

This creates:

```text
Tools/Prism/examples/minimal/build/compile_commands.json
```

Then generate the AI set:

```bash
prism run \
    --repo-root Tools/Prism/examples/minimal \
    -p Tools/Prism/examples/minimal/build \
    -o Tools/Prism/examples/minimal/.prism/prism.raw.json \
    --out-dir Tools/Prism/examples/minimal/.prism
```

Expected outputs:

```text
Tools/Prism/examples/minimal/.prism/prism.raw.json
Tools/Prism/examples/minimal/.prism/prism.graph.json
Tools/Prism/examples/minimal/.prism/prism.txt
```

### Fast Path - Generate the AI Set

From the repository root, after `build/compile_commands.json` exists:

```bash
prism run
```

Default behavior:

| Field | Default |
|-------|---------|
| Repository root | `.` |
| Compile database dir | `build/` |
| Raw extraction | `prism.raw.json` |
| Graph JSON | `prism.graph.json` |
| Text summary | `prism.txt` |

Use an explicit output directory when you want Prism artifacts grouped:

```bash
prism run --repo-root . -p build -o .prism/prism.raw.json --out-dir .prism
```

The output set is:

| File | Purpose |
|------|---------|
| `prism.raw.json` | Direct Clang extractor output; stable C++ to Python boundary. |
| `prism.graph.json` | Dependency-resolved AI memory graph. |
| `prism.txt` | Human-readable summary for quick inspection. |

### Controlled Run

Add options only when the default path is not enough.

For the minimal example, run CMake first so Prism can read the exact include
paths from `compile_commands.json`:

```bash
cmake \
    -S Tools/Prism/examples/minimal \
    -B Tools/Prism/examples/minimal/build \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Then run Prism with custom output names:

```bash
prism run \
    --repo-root Tools/Prism/examples/minimal \
    -p Tools/Prism/examples/minimal/build \
    -o Tools/Prism/examples/minimal/.prism/custom.raw.json \
    --out-dir Tools/Prism/examples/minimal/.prism \
    --json-name custom.graph.json \
    --txt-name custom.txt \
    Tools/Prism/examples/minimal/src/SampleComponent.cpp
```

`prism run` is only orchestration. It runs `prism-extract` first, then
passes the raw JSON to `prism-graph`. If extraction fails, graph generation
does not run.

### Low-Level Tools

The commands below assume the minimal example compile database already exists.
If it does not, run the CMake command from `Minimal Example First`.

Use `prism extract` when you only want the Clang extraction stage for
specific translation units:

```bash
prism extract \
    --repo-root Tools/Prism/examples/minimal \
    -o Tools/Prism/examples/minimal/.prism/prism.raw.json \
    -p Tools/Prism/examples/minimal/build \
    Tools/Prism/examples/minimal/src/SampleComponent.cpp
```

Use `prism graph` when you already have raw extractor output:

```bash
prism graph Tools/Prism/examples/minimal/.prism/prism.raw.json

prism graph Tools/Prism/examples/minimal/.prism/prism.raw.json \
    --out-dir Tools/Prism/examples/minimal/.prism \
    --no-txt
```

The underlying binaries remain available for automation that wants exact
stage names:

```bash
Tools/Prism/bin/prism-extract --help
Tools/Prism/bin/prism-graph --help
```

### Tests

Run the lightweight Prism CLI tests without LLVM or a compile database:

```bash
python3 Tools/Prism/tests/run.py
```

---

## Graph Schema (prism.graph.json)

```jsonc
{
  "schema_version": "1.0",
  "generated_by":   "prism-graph 1.0.0",
  "repo_root":      "/abs/path/to/MyCppProject",

  "stats": {
    "total_symbols": 8741,
    "total_files":   412,
    "total_lines":   186300,
    "by_kind":     { "CXXRecord": 340, "Function": 2140, "CXXMethod": 4810, … },
    "by_category": { "headers": 218, "sources": 194, "cmake": 12, "others": 8 }
  },

  "modules": {
    "Runtime/Renderer": {
      "files":         ["Runtime/Renderer/GBuffer.h", …],
      "symbol_count":  143,
      "include_count": 28
    }
  },

  "files": {
    "Runtime/Renderer/RenderGraph.h": {
      "module":     "Runtime/Renderer",
      "category":   "headers",
      "line_count": 312,
      "brief":      "Deferred render graph for the main pass.",
      "includes":   ["Runtime/Core/Handle.h", …],
      "symbol_ids": ["a1b2c3d4e5f6a7b8", …]
    }
  },

  "symbols": {
    "a1b2c3d4e5f6a7b8": {
      "name":           "RenderGraph",
      "qualified_name": "MyProject::Renderer::RenderGraph",
      "kind":           "CXXRecord",
      "access":         "public",
      "brief":          "Deferred lighting render graph.",
      "file":           "Runtime/Renderer/RenderGraph.h",
      "line":           42,
      "is_definition":  true,
      "bases":          ["IRenderPass"],
      "deps":           ["b2c3d4e5f6a7b8c9"],
      "children":       []
    }
  }
}
```

---

## Dependency Resolution

The extractor records outbound dependencies as **USR strings** (Clang's Unified
Symbol Resolution - a canonical, stable identifier for every declaration).

The Python builder resolves USR strings to stable graph IDs after all
translation units have been parsed. USRs that do not resolve to a known symbol
(system types, external libraries) are dropped rather than propagated as
dead references.

---

## Why this split?

**C++ core** - LibTooling gives correct template resolution, macro expansion,
and access to the full Clang type system. These are not approximations: the same
compiler that builds the project parses it for Prism.

**Python shell** - deduplication, dependency resolution, and aggregation are
pure data transformations. Python is faster to iterate and adds zero correctness
risk. The two layers share only a versioned JSON schema.

**Stable boundary** - `Record.h` and the JSON schema are the only public
contracts. Either layer can be rewritten, moved to a separate repository, or
run as a daemon without touching the other.
