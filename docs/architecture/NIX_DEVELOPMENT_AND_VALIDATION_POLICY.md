# Nix Development And Validation Policy

> Last updated: 2026-06-22

This document defines SagaEngine's current Nix boundary.

Nix is allowed to be the preferred development and validation environment.
NixOS is not allowed to become the engine platform requirement.

## Policy

The short form is:

```text
Development environment: Nix-first is acceptable.
Linux renderer validation: Vulkan-first is acceptable.
Engine platform: NixOS-first is not acceptable.
```

Nix gives the project a reproducible shell for builds, tests, tool versions,
and CI-like local validation. It is a validation wrapper around the engine, not
an engine runtime dependency.

Linux graphics validation may prioritize the Vulkan backend because it is the
current canonical GPU path. That does not make Linux support NixOS-only, and it
does not allow production code to depend on NixOS-specific services or paths.

## Documentation Rules

Durable documentation must avoid pinning changing build output directories such
as:

```text
build/RelWithDebInfo-0.0.x
```

Use placeholders for active build directories instead:

```bash
nix-shell --run "cmake --build <build-dir> --target SagaSandbox"
nix-shell --run "<build-dir>/bin/SagaSandbox first_3d_vertical_slice --vulkan"
```

`<build-dir>` means the currently configured build directory for the branch or
release being validated. Release notes, local logs, or historical evidence may
include exact versioned paths when recording what was actually run, but
architecture and how-to documents should not make those paths look canonical.

## Engineering Boundary

Engine source, installed headers, runtime APIs, assets, and renderer contracts
must not require:

- NixOS as the host operating system;
- Nix store paths such as `/nix/store/...`;
- Nix daemon behavior;
- NixOS service names or filesystem layout;
- shell-only assumptions that prevent equivalent non-Nix builds.

Project-maintained validation commands may use `nix-shell --run` when that is
the clearest reproducible path. A developer without Nix may need to provide the
same compiler, SDK, Vulkan loader, and toolchain dependencies by other means,
but the engine architecture should remain independent of NixOS.

## Validation Claims

A passing Nix-based validation run proves the code works in the project
standard validation environment. It does not prove that NixOS is the product
platform.

A Vulkan-first Linux renderer test proves the current canonical Linux GPU path.
It does not exclude other Linux distributions, future renderer backends, or
non-Nix packaging.

When documenting verification commands:

- prefer `nix-shell --run` for reproducible contributor validation;
- use `<build-dir>` instead of versioned build folders in durable docs;
- state Vulkan as the current Linux renderer proof path when relevant;
- do not describe NixOS as a requirement unless a specific host-only tool truly
  requires it.

## Non-Goals

This policy does not remove Nix from development workflows.

It does not require every contributor to have a fully supported non-Nix setup
today.

It does not make Vulkan the only future renderer backend.

It only prevents the engine's platform story from quietly becoming NixOS-first
through documentation, paths, build scripts, or runtime assumptions.
