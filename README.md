# SagaEngine

SagaEngine is a foundation for an MMO-first engine core.

It is not a platform product.
It is not production-ready.

The project is intentionally structured around:
- MMO runtime and authoritative server boundaries
- replication and interest management
- persistence and backend service separation
- moddability and scripting boundaries
- future platform expansion without hard-coding platform behavior into the core

The current focus is on building a clean technical base that can support:
- a client runtime
- a dedicated server runtime
- backend services
- eventual modding and content delivery workflows

## What this project is

- A core engine foundation
- MMO-oriented by design
- Architecture-first and extension-friendly
- Built to separate runtime, server, and backend concerns early

## What this project is not

- A finished game platform
- A Roblox-like ecosystem today
- A production-ready engine
- A generic engine meant to solve every use case

## Current status

- Architectural maturity: around 74%
- Platform coverage: Windows-first
- Production readiness: not ready
- Scope: actively evolving

## Repository structure

- `Engine/`  
  Core runtime systems: ECS, rendering, RHI, simulation, input, physics, scripting, resources, platform interfaces

- `Runtime/`  
  Client runtime glue and client-side networking/input flow

- `Server/`  
  Dedicated server runtime, replication orchestration, interest handling, shard and zone logic

- `Backends/`  
  Persistence and backend service infrastructure

- `Editor/`  
  Editor tooling

- `Tests/`  
  Unit, integration, and stress tests

## Tools

### Prism — AI Memory Graph Builder

Prism transforms C++ source code into a structured, AI-consumable memory graph. It operates in two layers connected by a versioned JSON schema:

**prism-extract** — C++ binary using Clang LibTooling to parse translation units, extract declarations with documentation, type relationships, and include graphs.

**prism-graph** — Python pipeline that deduplicates symbols, resolves dependencies, aggregates modules, and exports the final graph.

#### Capabilities

- Parse C++ codebase using real compiler frontend (Clang)
- Extract declarations with Doxygen/triple-slash documentation
- Build symbol dependency graph with USR-based resolution
- Generate machine-readable JSON graph (`prism.graph.json`)
- Produce human-readable summary (`prism.txt`)
- Support incremental extraction and parallel processing
- Output stable 16-character SHA-1 IDs from Clang USR strings

#### Installation

Prism now uses a unified build script for simplified installation:

```bash
# Full installation (requires LLVM 16+ and CMake 3.22+)
python3 Tools/Prism/build.py

# Python-only installation (no LLVM required)
python3 Tools/Prism/build.py --skip-extractor

# Debug build with clean rebuild
python3 Tools/Prism/build.py --debug --clean

# Explicit LLVM path (if auto-detection fails)
python3 Tools/Prism/build.py --llvm-dir /usr/lib/llvm-18/lib/cmake/llvm
```

After installation, binaries are staged in `Tools/Prism/bin/`:
- `prism-extract` — C++ extractor (requires LLVM)
- `prism-graph` — Python pipeline (always available)

#### Usage

**Step 1: Extract symbols from C++ source**

```bash
# Process entire repository using compile_commands.json
Tools/Prism/bin/prism-extract \
    --repo-root /path/to/SagaEngine \
    -o prism.raw.json \
    -p build/

# Process specific files
Tools/Prism/bin/prism-extract \
    --repo-root /path/to/SagaEngine \
    -o prism.raw.json \
    -p build/ \
    Engine/Renderer/RenderGraph.cpp
```

**Step 2: Build the dependency graph**

```bash
# Using staged launcher (recommended)
Tools/Prism/bin/prism-graph prism.raw.json

# Full options via direct call
python3 Tools/Prism/pipeline/run.py prism.raw.json \
    --out-dir output/ \
    --no-txt

# Verbose mode for debugging
Tools/Prism/bin/prism-graph prism.raw.json --verbose
```

#### Output Formats

**prism.graph.json** — Machine-readable AI memory graph containing:
- Symbol nodes with qualified names, kinds, access levels, documentation
- File nodes with include graphs and line counts
- Module aggregation (e.g., `Engine/Renderer`)
- Dependency resolution via USR strings
- Statistics (total symbols, files, distribution by kind)

**prism.txt** — Human-readable summary with module sections and symbol tables

#### Requirements

| Component    | Minimum Version |
|--------------|-----------------|
| CMake        | 3.22            |
| C++ Compiler | C++17           |
| LLVM/Clang   | 16              |
| Python       | 3.11            |

For detailed architecture, schema documentation, and roadmap, see `Tools/Prism/README.md` and `Tools/Prism/PRISM_ROADMAP.md`.

---

## Build system

### Windows

```powershell
cd Tools/Forge

.\build.ps1 lock -Profile windows-msvc

.\build.ps1 setup -Profile windows-msvc -Preset windows-msvc-14.38

.\build.ps1 build -Profile windows-msvc -Preset windows-msvc-14.38

.\build.ps1 test -Preset windows-msvc-14.38
```

### Linux / macOS (Prism and Forge)

```bash
# Build Prism (C++ extractor + Python pipeline)
python3 Tools/Prism/build.py

# Build Forge (build system tool)
python3 Tools/Forge/tool/build.py

# Add tools to PATH
export PATH="$PWD/Tools/Prism/bin:$PWD/Tools/Forge/tool/bin:$PATH"
```

### Cross-Platform Notes

- **NixOS**: Both build scripts support automatic `nix-shell` detection and dependency validation
- **LLVM Detection**: `build.py` scans FHS paths, uses `llvm-config`, and supports NixOS store glob patterns
- **Fallback Behavior**: Missing LLVM skips `prism-extract` but still stages `prism-graph`