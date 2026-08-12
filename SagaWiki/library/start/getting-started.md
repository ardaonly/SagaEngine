---
title: Getting started
status: guide
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Getting started

Read the repository as a set of owned modules and contracts before treating it as a packaged end-user product. The live Linux workflow uses the `linux-gcc` preset; Forge owns dependency setup and CMake owns configure and build.

## First checks

For a fresh clone, initialize submodules before dependency setup:

```sh
git status --short
git submodule update --init --recursive
nix-shell --run "Tools/Forge/bin/forge install --profile linux-gcc"
nix-shell --run "cmake --preset linux-gcc"
nix-shell --run "cmake --build --preset linux-gcc --parallel 2"
```

Run the desktop product from the configured build:

```sh
build/RelWithDebInfo-0.0.11/bin/Saga
```

The normal bounded validation entry point is:

```sh
nix-shell --run "python3 Tools/Developer/CI/run_suite.py quick"
```

It configures, builds its declared prerequisites with two jobs, and runs only
the CMake-owned quick selection. The suite configure disables compiler launchers
because the current GCC dependency scanner and sccache combination can lose
depfiles during parallel focused builds. Full local CTest is not the normal
developer path.

Product UI validation is explicit:

```sh
nix-shell --run "python3 Tools/Developer/CI/run_suite.py product-xcb"
```

It requires Qt/XCB and either an existing `DISPLAY` or Xvfb. It is not a GPU
test and is not included in `quick`.

## Project inputs

A `.sagaproj` manifest identifies project roots and package inputs. SagaProjectKit validates the manifest contract, while SagaPackager consumes the same manifest boundary when staging packages. A sample manifest is an example and evidence fixture, not an alternate schema authority.

For scripts, C# source is authoritative. Visual Blocks and generated descriptors are views or artifacts over that source; they do not create a second behavior graph. See [C# and Visual Blocks](../editor/csharp-and-visual-blocks.md).

## Useful wrappers

```sh
scripts/build
scripts/test
scripts/verify
scripts/package
scripts/wiki build
scripts/wiki verify
```

These root wrappers are routing surfaces. Product logic stays in the owning tools and modules.

## Managed runtime and recovery

Saga packages a pinned private .NET runtime with SagaProjectKit, SagaScript,
and SagaPackager. Do not configure a shipped product to probe a global `dotnet`.
The developer .NET SDK used while compiling those tools is a separate build
prerequisite.

On failure, keep the first diagnostic and suite manifest under
`build/.../reports/test-suites`. Check `git status`, the selected preset,
submodule state, available disk/memory, and declared environment requirements.
Re-run the smallest affected suite. Do not delete arbitrary project, package,
or build state as a universal recovery step; use a separate clean build root
when stale-build diagnosis is actually required.

## Avoid stale assumptions

- Do not use retired top-level source roots such as `Runtime/`, `Server/`, `Backends/`, or `Apps/`.
- Do not treat historical `docs/` paths as current documentation.
- Do not infer production readiness from target names.
- Do not edit generated wiki HTML; edit Markdown under `SagaWiki/library/` and rebuild.

## Detailed references

- [Product and capability reference](../reference/product-and-capability-boundaries.md)
- [Runtime lifecycle, assets, and packages](../reference/runtime-lifecycle-assets-and-packages.md)
- [Build, package, and distribution contracts](../reference/build-package-and-distribution-contracts.md)
