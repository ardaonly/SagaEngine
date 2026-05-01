# Prism

> **C++ codebase → AI memory graph.**  
> Real compiler frontend. Structured output. No regex.

---

## What it does

Prism parses every C++ translation unit in a repository using the Clang
compiler frontend, extracts declarations with their documentation, type
relationships, and include graph, then builds a dependency-resolved symbol
graph that AI assistants can navigate directly.

The output is a JSON graph file — not a raw code dump — structured for
efficient AI consumption with no context wasted on boilerplate.

---

## Architecture

```
prism/
│
├── extractor/                    C++ binary: prism-extract
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp              ClangTool entry — options, run, emit
│       ├── Record.h              POD structs — the only contract between layers
│       │
│       ├── Support/
│       │   ├── PathUtils         Path relativization and system-header detection
│       │   ├── IdGen             Stable 16-char SHA-1 IDs from Clang USR strings
│       │   └── CommentParser     Brief extraction from Doxygen / triple-slash blocks
│       │
│       ├── AST/
│       │   ├── DeclExtractor     NamedDecl → SymbolRecord (all Clang API calls here)
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
        │   ├── loader.py         Defensive JSON → RawExtraction deserialization
        │   ├── builder.py        Five-phase graph construction (dedup → resolve → aggregate)
        │   └── classifier.py     File category assignment and module name derivation
        └── export/
            ├── base.py           Abstract Exporter interface
            ├── json_exporter.py  → prism.graph.json (public AI contract)
            └── txt_exporter.py   → prism.txt (human-readable summary)
```

---

## Data Flow

```
C++ source files
       │
       ▼  Clang LibTooling — real AST, real type system
prism-extract
       │
       ▼  stable JSON — no Clang types cross this boundary
prism.raw.json
       │
       ▼  Python — dedup, USR resolution, module aggregation
prism-graph
       │
       ├──▶  prism.graph.json    machine-readable AI memory graph
       └──▶  prism.txt           human-readable summary
```

---

## Build

### Requirements

| Component     | Minimum version |
|---------------|-----------------|
| CMake         | 3.22            |
| C++ compiler  | C++17           |
| LLVM / Clang  | 16              |
| Python        | 3.11            |

### Steps

```bash
# Configure — supply your LLVM installation paths
cmake -B build \
    -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm \
    -DClang_DIR=/usr/lib/llvm-18/lib/cmake/clang \
    -DCMAKE_BUILD_TYPE=Release

# Build the extractor binary
cmake --build build --target prism-extract -j$(nproc)
```

---

## Usage

### Step 1 — Extract

```bash
# Process all TUs listed in compile_commands.json
./build/extractor/prism-extract \
    --repo-root /abs/path/to/SagaEngine \
    -o prism.raw.json \
    -p build/

# Process specific files
./build/extractor/prism-extract \
    --repo-root /abs/path/to/SagaEngine \
    -o prism.raw.json \
    -p build/ \
    Engine/Renderer/RenderGraph.cpp \
    Engine/Renderer/GBuffer.cpp
```

### Step 2 — Build graph

```bash
# Full run — writes prism.graph.json + prism.txt
python pipeline/run.py prism.raw.json

# Custom output directory, JSON only
python pipeline/run.py prism.raw.json \
    --out-dir output/ \
    --no-txt

# Verbose output for debugging
python pipeline/run.py prism.raw.json --verbose
```

---

## Graph Schema (prism.graph.json)

```jsonc
{
  "schema_version": "1.0",
  "generated_by":   "prism-graph 1.0.0",
  "repo_root":      "/abs/path/to/SagaEngine",

  "stats": {
    "total_symbols": 8741,
    "total_files":   412,
    "total_lines":   186300,
    "by_kind":     { "CXXRecord": 340, "Function": 2140, "CXXMethod": 4810, … },
    "by_category": { "headers": 218, "sources": 194, "cmake": 12, "others": 8 }
  },

  "modules": {
    "Engine/Renderer": {
      "files":         ["Engine/Renderer/GBuffer.h", …],
      "symbol_count":  143,
      "include_count": 28
    }
  },

  "files": {
    "Engine/Renderer/RenderGraph.h": {
      "module":     "Engine/Renderer",
      "category":   "headers",
      "line_count": 312,
      "brief":      "Deferred render graph for the main pass.",
      "includes":   ["Engine/Core/Handle.h", …],
      "symbol_ids": ["a1b2c3d4e5f6a7b8", …]
    }
  },

  "symbols": {
    "a1b2c3d4e5f6a7b8": {
      "name":           "RenderGraph",
      "qualified_name": "SagaEngine::Renderer::RenderGraph",
      "kind":           "CXXRecord",
      "access":         "public",
      "brief":          "Deferred lighting render graph.",
      "file":           "Engine/Renderer/RenderGraph.h",
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
Symbol Resolution — a canonical, stable identifier for every declaration).

The Python builder resolves USR strings to stable graph IDs after all
translation units have been parsed. USRs that do not resolve to a known symbol
(system types, external libraries) are dropped rather than propagated as
dead references.

---

## Why this split?

**C++ core** — LibTooling gives correct template resolution, macro expansion,
and access to the full Clang type system. These are not approximations: the same
compiler that builds the engine parses it for Prism.

**Python shell** — deduplication, dependency resolution, and aggregation are
pure data transformations. Python is faster to iterate and adds zero correctness
risk. The two layers share only a versioned JSON schema.

**Stable boundary** — `Record.h` and the JSON schema are the only public
contracts. Either layer can be rewritten, moved to a separate repository, or
run as a daemon without touching the other.