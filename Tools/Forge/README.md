## Forge Purpose and Boundaries

Forge exists to remove the repetitive friction from everyday C++ development without replacing the underlying build ecosystem.

Its job is to provide a single, predictable developer interface for:
- dependency installation,
- project configuration,
- builds,
- test execution,
- and toolchain enforcement.

Forge is intentionally not:
- a replacement for CMake,
- a replacement for Conan,
- a package registry,
- a new build language,
- or a general-purpose task runner.

Forge is Apache-2.0 under Arda Koyuncu. The SagaEngine monorepo is the
canonical source; standalone Forge repositories are mirrors or release exports.

The project succeeds only if it:
- reduces the number of commands required for normal development,
- preserves escape hatches for advanced users,
- stays backend-neutral internally,
- and remains extractable as a standalone tool.

If a feature does not improve day-to-day developer workflow, reproducibility, or backend portability, it does not belong in Forge.

Forge is an orchestration layer for CMake and Conan designed to provide a consistent, Cargo-inspired workflow for C++ projects. It wraps CMake and Conan under a single, ergonomic command set designed to collapse the everyday build sequence — dependency resolution, project configuration, and compilation — into predictable commands while preserving full access to the underlying tools.

Forge is not a replacement for CMake or Conan. It is an orchestration layer that sits above them. Complex build logic stays in CMake; dependency declarations stay in `conanfile.py` or `forge.toml`; Forge coordinates the workflow.

Forge owns workflow coordination and execution policy.

CMake owns build graph generation.
Conan owns dependency resolution.
The compiler owns compilation semantics.
Forge coordinates these systems into a reproducible developer workflow.

---

## Architecture

Forge exposes three levels of control:

| Level | Example |
|-------|---------|
| High-level | `forge install --profile windows-msvc` / `forge build` |
| Intermediate | `forge build --config Debug --target SagaEditor` |
| Low-level | `forge run cmake --build build --target SagaEngine` |

At its core, Forge maintains an internal, backend-neutral build model. The `forge.toml` manifest is not passed directly to CMake or Conan; it is first resolved into this intermediate representation and then lowered into concrete backend invocations. This keeps the CLI surface stable even if backends change in the future.

---

## Directory Layout

```
Tools/Forge/
├── FORGE_ROADMAP.md   — development status, shipped and open items
├── README.md          — this file
└── tool/              — standalone Forge binary source
    ├── CMakeLists.txt — self-contained build (no Conan, no marker file)
    ├── include/Forge/ — public headers (Manifest, ProcessRunner)
    ├── src/           — CLI implementation
    └── build.py       — Python 3 bootstrap installer
```

The `tool/` subtree is the entirety of Forge. It contains no SagaEngine headers, no references to `SagaEngineRoot.marker`, and no third-party library dependencies. The contents of `tool/` can become a standalone repository root without any structural rewrite.

Engine build infrastructure (CMakeLists.txt, CMakePresets.json, conanfile.py, profiles/, cmake/modules/) lives at the repository root. It describes how SagaEngine is built; it is not part of Forge.

---

## Building Forge

Prerequisites: Python 3 (stdlib only), CMake 3.22 or later.

```sh
python3 Tools/Forge/tool/build.py
```

This configures and builds Forge using the system CMake, then stages the binary to `Tools/Forge/tool/bin/forge`. Add that directory to `PATH` or copy the binary to a location already on `PATH`.

Options: `--debug`, `--clean`, `--jobs <n>`.

---

## Using Forge with SagaEngine

The engine root contains a `forge.toml` that declares the project and toolchain pins. The dependency graph is managed by `conanfile.py`. The recommended workflow:

**Windows (from the repository root):**

```powershell
# Initializes MSVC environment and delegates to forge
.\build.ps1 install --profile windows-msvc
.\build.ps1 configure --preset windows-msvc-14.38
.\build.ps1 build
```

**Linux / macOS (forge must be on PATH):**

```sh
forge install
forge configure --preset <preset-name>
forge build
```

**All platforms (low-level, explicit):**

```sh
forge run conan install . --profile:host=profiles/windows-msvc --profile:build=profiles/windows-build --build=missing
forge configure --preset windows-msvc-14.38
forge build
```

---

## Command Reference

| Command | Description |
|---------|-------------|
| `forge new <dir>` | Scaffold a new project |
| `forge init` | Write `forge.toml` in the current directory |
| `forge add <pkg>[@<ver>]` | Append a dependency to `forge.toml` |
| `forge install [--profile <name>]` | Install dependencies via Conan |
| `forge configure [--preset=NAME]` | Configure via CMake |
| `forge build [--target=NAME] [--config=Release]` | Build via CMake |
| `forge clean` | Remove the build directory |
| `forge run <executable> [args]` | Escape hatch: run any binary on PATH |
| `forge --version` | Print Forge version |
| `forge --help` | Print usage |

`--strict` is accepted by every build and install command. It will enforce toolchain pin verification once that roadmap item ships.

---

## Isolation Guarantee

Forge satisfies the following invariants. Any violation blocks merge:

- No `#include` of any SagaEngine, SagaEditor, SagaServer, or SagaPrism header.
- No walk-up search for `SagaEngineRoot.marker` or any engine-specific marker file.
- No linkage against any third-party library; the `forge` binary depends only on the C++ standard library.
- No reading of the SagaTools dispatcher registry.
- `cmake` and `conan` are spawned only for declared subcommands. Arbitrary executables are spawned only via the explicit `forge run` escape hatch.

---

## Roadmap

See [FORGE_ROADMAP.md](FORGE_ROADMAP.md) for the full development status, including shipped items, open items, and post-1.0 deferred work.
