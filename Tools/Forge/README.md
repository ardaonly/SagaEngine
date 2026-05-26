# Forge

> Standalone build workflow tool for C++ projects that use CMake and Conan.

Forge removes repetitive build friction without replacing the tools that already
define a C++ project. CMake owns the build graph. Conan owns dependency
resolution. The compiler owns compilation semantics. Forge coordinates the
normal sequence around them through one predictable command line.

Forge is Apache-2.0 under Arda Koyuncu. This directory is the standalone Forge
source root and can be exported as its own repository.

---

## Quick Start

Build Forge:

```sh
python3 Tools/Forge/build.py
```

The binary is staged at `Tools/Forge/bin/forge`. Add that directory to `PATH`,
or invoke the binary directly.

Create or update a CMake project:

```sh
forge init
forge add fmt@10.2.1
forge install
forge configure
forge build
```

When you need direct backend control, use the escape hatch:

```sh
forge run conan install . --build=missing
forge run cmake --build build --target MyGame
```

---

## What Forge Owns

Forge provides a consistent interface for:

- dependency installation,
- project configuration,
- builds,
- test execution,
- install targets,
- formatting,
- environment reporting,
- and strict toolchain validation.

Forge is intentionally not:

- a replacement for CMake,
- a replacement for Conan,
- a package registry,
- a new build language,
- a plugin host,
- or a general-purpose task runner.

If a feature does not improve day-to-day developer workflow, reproducibility, or
backend portability, it does not belong in Forge.

---

## Command Levels

Forge exposes three levels of control:

| Level | Example |
|-------|---------|
| High-level | `forge add raylib@5.0` / `forge install` / `forge build` |
| Intermediate | `forge build --config Debug --target MyGame` |
| Low-level | `forge run cmake --build build --target MyGame` |

The low-level path is deliberate. Forge should reduce the default command
sequence, not hide CMake or Conan from the project.

---

## Examples

Small examples live under `examples/`.

| Example | Purpose |
|---------|---------|
| `examples/hello-cmake` | Minimal CMake project with no dependencies. |
| `examples/raylib-conan` | Small raylib project using `forge add`, `forge install`, CMake, and Conan together. |

Typical example flow after `forge` is on `PATH`:

```sh
cd Tools/Forge/examples/hello-cmake
forge configure
forge build
forge run ./build/hello-cmake
```

For the raylib example:

```sh
cd Tools/Forge/examples/raylib-conan
forge add raylib@5.0
forge install
forge configure
forge build
```

---

## Directory Layout

```text
Tools/Forge/
├── CMakeLists.txt          standalone Forge build
├── README.md               this file
├── FORGE_ROADMAP.md        development status
├── SCHEMA.md               forge.toml schema reference
├── build.py                Python bootstrap installer
├── build.cmd               Windows bootstrap shim
├── include/Forge/          public internal headers
├── src/                    CLI and backend adapters
└── examples/               small usage examples
```

Project-specific build infrastructure such as the engine root `CMakeLists.txt`,
`CMakePresets.json`, `conanfile.py`, profiles, and custom CMake modules belongs
to the consuming repository. It is not part of Forge.

---

## Command Reference

| Command | Description |
|---------|-------------|
| `forge new <dir>` | Scaffold a new CMake project |
| `forge init` | Write `forge.toml` in the current directory |
| `forge add <pkg>[@<ver>]` | Append or update a dependency in `forge.toml` |
| `forge install [--profile <name>]` | Install dependencies via Conan |
| `forge configure [--preset=NAME]` | Configure via CMake |
| `forge build [--target=NAME] [--config=Release]` | Build via CMake |
| `forge test [--label=LABEL] [--suite=NAME] [--jobs=N]` | Run CTest |
| `forge install-target [--prefix=DIR]` | Run `cmake --install` |
| `forge presets [build\|test\|configure]` | List CMake presets |
| `forge fmt [--source=DIR]` | Run clang-format over project sources |
| `forge env [--json]` | Print detected tool versions |
| `forge clean` | Remove the build directory |
| `forge run <executable> [args]` | Run any binary on `PATH` explicitly |

`--strict` enables CI-oriented validation for toolchain pins and manifest
sections. `--explain` prints the backend command without executing it where
supported.

---

## Repository Integration: SagaEngine

SagaEngine uses Forge as its preferred build interface. The engine root owns its
own `forge.toml`, `conanfile.py`, profiles, presets, and CMake modules.

Windows from a Visual Studio Developer PowerShell:

```powershell
forge install --profile windows-msvc
forge configure --preset windows-msvc-14.38
forge build
```

The repository root `build.ps1` is only a legacy compatibility wrapper. It is not
the preferred Windows entrypoint.

Linux / macOS after Forge is on `PATH`:

```sh
forge install
forge configure --preset <preset-name>
forge build
```

Forge uses conservative resource-aware scheduling by default. Engine repositories
can also pin a lower default with `[build] jobs = 2`; users may request a higher
count with `--jobs=N`, but Forge still clamps it through CPU/RAM safety limits.
Use `--force-unsafe-jobs` only for deliberate local experiments where memory
pressure is acceptable.

On NixOS, run project toolchain commands from `nix-shell`, or use the explicit
wrapper form:

```sh
forge nix install --profile linux-gcc
```

Normal `forge install/configure/build` commands fail fast outside `nix-shell`
instead of silently re-entering the environment. This keeps recursion, CI, and
environment drift visible.

---

## Isolation Guarantee

Forge satisfies these invariants:

- No `#include` of host-repository engine, editor, server, or tool headers.
- No walk-up search for host-specific marker files.
- No runtime dependency on third-party libraries.
- No repository-specific tool registry.
- `cmake` and `conan` are spawned only for declared subcommands.
- Arbitrary executables are spawned only via `forge run`.

See [FORGE_ROADMAP.md](FORGE_ROADMAP.md) for shipped items and planned work.
