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

Each tool below is structured as an independent, repo-extractable project. The
engine consumes them only through their public packaging contracts (Conan
packages or installed CMake config); none of them are absorbed into the engine
build tree.

- **Prism** — C++ source to AI memory graph. See `Tools/Prism/README.md`.
- **Forge** — Cargo-flavored build frontend (CMake + Conan). See `Tools/Forge/README.md`.
- **SDE** — Model definition, validation, and compilation pipeline. Standalone
  C++ static library packaged as `sde/0.1.1`. The engine links against it only
  when `SAGA_WITH_SDE=ON` (CMake) or `-o &:with_sde=True` (Conan) is set.
  See `Tools/SystemDefinitionEngine/README.md`.

## Licensing and boundaries

SagaEngine uses the monorepo as the only canonical development source. Prism,
Forge, and SDE may be mirrored or exported as standalone repositories, but they
do not override the monorepo state.

The engine core and toolchain are Apache-2.0. The Editor is source-available
under a separate read-only, no-AI-training license. See `LICENSE.md` and
`core/manifest/path_rules.json` for the current path inventory.

Phase 1 boundary tooling is observation-only: it reports drift without turning
the repository into a policy engine.

Run the boundary report locally with:

```sh
python3 Tools/scripts/report_boundary_status.py --repo-root .
python3 Tools/scripts/check_qt_boundary.py .
python3 Tools/scripts/check_engine_server_boundary.py --repo-root .
```

The Engine-to-Server check enforces zero `SagaServer` references in public
`SagaEngine` headers. Packet primitives are engine-owned; shared replication
wire data lives in `SagaShared`.

## Tool mirror export

Tool mirror exports are data-driven from `core/export/manifest.json`. Prism,
Forge, and SDE are split from the monorepo with `git subtree split` and pushed
to their configured mirror repositories.

```sh
./export.sh --dry-run
./export.sh --tool Prism
./export.sh --since HEAD~1
```

The export state cache is written under `.core/export/state/` and is ignored by
git. GitHub Actions uses the same manifest, so adding another mirrorable tool is
a manifest-only change plus creating the target repository/token access.

## Host automation

`Tools/Host` provides a small Docker Compose wrapper for local server hosting:

```sh
tools host start
tools host stop
tools host restart
tools host rebuild
tools host logs
tools host status
```

The direct script entry point is `Tools/Host/host.sh`. It manages Postgres,
Redis, and the local `SagaServer` container without deleting Docker volumes as
part of normal stop/restart commands.

## Build system

SagaEngine is built through **Forge**. See `Tools/Forge/README.md` for full documentation.

### Prerequisites

- Python 3 (stdlib only) — required to bootstrap Forge
- CMake 3.22 or later
- Conan 2.0 or later
- A C++ toolchain matching one of the profiles in `profiles/`
  (`windows-msvc`, `windows-clang`, `linux-gcc`, `macos-clang`)

### One-time: build Forge itself

Forge is a standalone binary. Build it once, then add it to your `PATH`.

```sh
python3 Tools/Forge/build.py
```

The binary is staged at `Tools/Forge/bin/forge`. Verify with:

```sh
forge --version
```

### Build the engine

Run all commands from the repository root.

**Windows (Visual Studio Developer PowerShell):**

```powershell
forge install --profile windows-msvc
forge configure --preset windows-msvc-14.38
forge build
```

`build.ps1` is kept only as a legacy compatibility wrapper for older automation.
New commands should call `forge` directly from a Visual Studio Developer shell.

**Linux:**

```sh
forge install --profile linux-gcc
forge configure --preset linux-gcc
forge build
```

On NixOS, run project toolchain commands from `nix-shell`, or use the explicit
wrapper form. The word after `forge nix` must be the Forge command to run:

```sh
forge nix install --profile linux-gcc
forge nix configure --preset linux-gcc
forge nix build
```

Do not run `forge nix --preset linux-gcc`; `--preset` is an option for the
`configure` command, so the command name must come first.

Normal `forge install/configure/build` commands fail fast outside `nix-shell`
instead of silently re-entering the environment.

The repository default is intentionally conservative: `forge.toml` sets
`jobs = 2`, and Forge clamps requested jobs through CPU/RAM safety limits.
Use `forge build --jobs=N` or `forge install --jobs=N` to request more
parallel work; use `--force-unsafe-jobs` only when you explicitly accept the
memory pressure risk.

**macOS:**

```sh
forge install --profile macos-clang
forge configure --preset macos-clang
forge build
```

### Common commands

```sh
# Build a specific target
forge build --target SagaEngine
forge build --target SagaEditor
forge build --target SagaRuntime
forge build --target SagaServer
forge build --target SagaDistribution

# Debug build
forge build --config Debug

# Wipe the build directory and start fresh
forge clean

# Add a dependency to forge.toml
forge add fmt@10.2.1

# Escape hatch: run cmake/conan/ninja directly through Forge
forge run cmake --build Build/windows-msvc-14.38 --target SagaEngine
forge run conan install . --profile:host=profiles/linux-gcc --build=missing
```

### Tips

- The first `forge install` is slow because Conan resolves and compiles
  third-party libraries from source. Subsequent runs reuse the local cache.
- Build artifacts go to `Build/<preset>/` — never to the source tree. Deleting
  that directory is always safe; `forge clean` does the same.
- The `RelWithDebInfo` configuration is the default for `windows-msvc-14.38`.
  Use `--config Debug` for symbol-rich builds when stepping through code.
- If a build breaks after pulling new commits, run
  `forge install` then `forge configure --preset <name>` before `forge build`;
  dependency or preset changes require re-resolution.
- Optional engine features are toggled at install time. To build with SDE
  support: `forge install -- -o &:with_sde=True`.
- Use `forge run <tool>` instead of invoking `cmake`, `conan`, or `ninja`
  directly — it guarantees the correct environment is loaded.
- `--strict` on `install` and `build` will enforce toolchain pin verification
  once the corresponding roadmap item ships; it is safe to pass today.

### Production Distribution

`SagaDistribution` stages the shipped SAGA product layout under
`build/RelWithDebInfo/dist/SAGA-<Platform>-v0.0.8/`. It is a composition target:
it does not compile engine logic of its own, and only packages existing shipped
artifacts.

The staged layout keeps role binaries in `bin/`; app-local runtime libraries are
placed under `lib/` on Unix-like platforms and next to the executables on
Windows where DLL lookup expects that shape.

Shipped role binaries:

- `Saga` — product orchestration entry point.
- `SagaEditor` — primary authoring product.
- `SagaRuntime` — runtime/player role.
- `SagaServer` — dedicated backend role.

`SagaClient` remains a legacy development alias and is never included in the
production distribution.
