---
title: Getting started
status: guide
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Getting started

Read the repository as a set of owned modules and contracts before treating it as a packaged end-user product. The live Linux workflow uses the `linux-gcc` preset; Forge owns dependency setup and CMake owns configure and build.

## First checks

From the repository root:

```sh
git status --short
scripts/wiki verify
nix-shell --run "Tools/Forge/bin/forge install --profile linux-gcc"
nix-shell --run "cmake --preset linux-gcc"
nix-shell --run "cmake --build --preset linux-gcc"
```

Build and test scope should match the change. Do not assume that a full CTest or GPU/stress run is appropriate for every local machine; use focused targets first and record exactly what was run. See [testing and evidence](../evidence/testing.md).

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

## Avoid stale assumptions

- Do not use retired top-level source roots such as `Runtime/`, `Server/`, `Backends/`, or `Apps/`.
- Do not treat historical `docs/` paths as current documentation.
- Do not infer production readiness from target names.
- Do not edit generated wiki HTML; edit Markdown under `SagaWiki/library/` and rebuild.

## Detailed references

- [Product and capability reference](../reference/product-and-capability-boundaries.md)
- [Runtime lifecycle, assets, and packages](../reference/runtime-lifecycle-assets-and-packages.md)
- [Build, package, and distribution contracts](../reference/build-package-and-distribution-contracts.md)
