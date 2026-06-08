# SagaEngine

SagaEngine is an active game engine and toolchain codebase. It focuses on
runtime/server boundaries, editor and authoring infrastructure, project
tooling, scripting analysis, packaging, diagnostics, and tests.

It is not a finished game engine product. Treat this repository as source for
engine and toolchain development, not as an installable editor for making games
end to end.

## Project Status

SagaEngine is currently in a temporary feature freeze.

No new major systems, phases, or architectural expansions are planned during this period. The current focus is to understand, audit, document, and stabilize the existing codebase.

This freeze does not mean the project is abandoned. Development is paused intentionally so the existing engine architecture, source files, public/private boundaries, build system, runtime flow, asset pipeline, rendering layer, server authority, and editor structure can be reviewed and properly owned before further expansion.

This project grew quickly through exploratory development, so the current priority is codebase comprehension, ownership review, and stabilization before adding more scope.

## SagaSandbox Preview

Current sandbox/render snapshots from the temporary frozen development state.

<p align="center">
  <img src="docs/media/sagasandbox/render-01.png" alt="SagaSandbox render preview 1" width="48%">
  <img src="docs/media/sagasandbox/render-02.png" alt="SagaSandbox render preview 2" width="48%">
</p>

## Current Position

The repo contains C++ engine, runtime, server, editor, application, tool, and
test code. It is currently strongest as a technical base for an engine and
authoring toolchain.

Major gaps remain:

- no beta or release-candidate status;
- no public SDK or production distribution;
- no finished editor workflow;
- no complete runtime gameplay or server gameplay loop;
- no complete visual scripting product or arbitrary C# round trip;
- no production networking, cloud, security, or scale validation.

## Repository Map

- `Engine/` - engine runtime systems, rendering/backend abstraction, scripting,
  resources, diagnostics, and platform-facing engine code.
- `Runtime/` - client runtime glue and client-side runtime integration.
- `Server/` - dedicated server runtime, replication, authority, zones, and
  server diagnostics.
- `Editor/` - editor models, panels, and authoring-side code.
- `Apps/` - application entry points and product roles.
- `Tools/` - build, project, scripting, packaging, diagnostics, and internal
  verification tools.
- `Tests/` - focused unit, integration, architecture, and stress-oriented tests.
- `samples/` - sample and fixture projects. `MultiplayerSandbox` is currently a
  deterministic fixture, not a finished multiplayer game. `StarterArena` is a
  project-definition sample only and is not playable.
- `docs/` - product status, architecture, roadmap, testing, and internal notes.

Start with [docs/README.md](docs/README.md). Older roadmap and recovery files
are background material, not the best source for current product status.

## Public Tooling Direction

The likely public CLI surface is intentionally small:

- `SagaScript` - C# / SagaScript analysis and source-authoring workflows.
- `SagaProjectKit` - project manifest validation and resolution.
- `SagaPackager` - package/profile checks and packaging-related workflows.

Many other tools in `Tools/` are internal diagnostics, policy, report, stress,
or recovery helpers. They should not be treated as product CLIs unless the docs
explicitly promote them.

## Build System

SagaEngine is built through Forge. See `Tools/Forge/README.md` for the detailed
tool documentation.

### Prerequisites

- Python 3, used to bootstrap Forge.
- CMake 3.22 or later.
- Conan 2.0 or later.
- A C++ toolchain matching one of the profiles in `profiles/`.

### Build Forge

```sh
python3 Tools/Forge/build.py
```

The binary is staged at `Tools/Forge/bin/forge`. Verify with:

```sh
Tools/Forge/bin/forge --version
```

### Configure And Build

Run commands from the repository root.

Linux:

```sh
Tools/Forge/bin/forge install --profile linux-gcc
Tools/Forge/bin/forge configure --preset linux-gcc
Tools/Forge/bin/forge build
```

NixOS:

```sh
nix-shell --run "Tools/Forge/bin/forge install --profile linux-gcc"
nix-shell --run "Tools/Forge/bin/forge configure --preset linux-gcc"
nix-shell --run "Tools/Forge/bin/forge build"
```

Windows:

```powershell
Tools/Forge/bin/forge install --profile windows-msvc
Tools/Forge/bin/forge configure --preset windows-msvc-14.38
Tools/Forge/bin/forge build
```

macOS:

```sh
Tools/Forge/bin/forge install --profile macos-clang
Tools/Forge/bin/forge configure --preset macos-clang
Tools/Forge/bin/forge build
```

### Useful Build Commands

```sh
Tools/Forge/bin/forge build --target SagaEngine
Tools/Forge/bin/forge build --target SagaEditor
Tools/Forge/bin/forge build --target SagaRuntime
Tools/Forge/bin/forge build --target SagaServer
```

Use `forge run <tool>` when invoking CMake, Conan, or Ninja through the same
environment Forge configured.

## Verification

There is no single universal green build/test command for every subsystem.
Prefer focused checks for the area you are reviewing.

Common local checks:

```sh
git diff --check
ctest --test-dir build/RelWithDebInfo-0.0.9 --output-on-failure
```

For NixOS-hosted checks, use `nix-shell --run "<command>"`.

## Distribution Staging

`SagaDistribution` stages a local product layout under the build directory. It
is a packaging target for existing artifacts. It is not a public release,
installer, marketplace package, or production support workflow.

Role binaries may include:

- `Saga`
- `SagaEditor`
- `SagaRuntime`
- `SagaServer`

`SagaClient` is a legacy development alias and is not part of the intended
distribution surface.
