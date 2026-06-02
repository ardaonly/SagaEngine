# SagaEngine

SagaEngine is a long-term game engine and toolchain project focused on
MMO-style runtime/server boundaries, authoring workflows, and buildable engine
infrastructure.

The repository is not a finished game engine product today. It is useful as an
engine/toolchain codebase for developers who understand the current limits; it
is not ready to be presented as an end-user game creation platform.

## Current Position

SagaEngine currently contains real C++ engine, runtime, server, editor,
application, tool, and test code. The strongest honest claim is that the repo is
building a serious technical base for an engine and authoring toolchain.

Current non-claims:

- no beta or release-candidate status;
- no SDK or distribution ready for production use;
- no finished editor workflow;
- no finished runtime gameplay or server gameplay loop;
- no complete visual scripting or round-tripping arbitrary C#;
- no production networking, cloud, security, or scale proof.

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

Start with [docs/README.md](docs/README.md) before relying on older roadmap or
historical documents.

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

The repo does not currently claim a universal green build/test matrix. Prefer
focused checks for the subsystem you are reviewing.

Common local checks:

```sh
git diff --check
ctest --test-dir build/RelWithDebInfo-0.0.9 --output-on-failure
```

For NixOS-hosted checks, use `nix-shell --run "<command>"`.

## Distribution Staging

`SagaDistribution` stages a local product layout under the build directory. It
is a packaging target for existing artifacts; it is not proof of public release
readiness, installer readiness, marketplace readiness, or production support.

Role binaries may include:

- `Saga`
- `SagaEditor`
- `SagaRuntime`
- `SagaServer`

`SagaClient` is a legacy development alias and is not part of the intended
distribution surface.
