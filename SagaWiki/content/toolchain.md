---
title: Toolchain and coding standards
status: current
owner: SagaWiki
generated_html: pages/toolchain.html
---

# Toolchain and coding standards

## Tool ownership

Forge owns dependency installation, configure, build, test, and environment orchestration. SagaTools is the stable dispatcher. SagaProjectKit validates project manifests. SagaScript owns C# tooling. SagaPackager owns package staging. `Tools/common` contains shared Python infrastructure; developer maintenance executables live below `Tools/Developer`. Stable scripts at the repository root delegate to these owners and contain no product logic, except `scripts/wiki`, which owns the local Markdown-to-HTML documentation workflow.

## C++ style and documentation

Saga-owned, hand-maintained C++ headers and sources begin with an exact `@file` and a precise one-sentence `@brief`. Public and architecturally meaningful types document ownership, lifetime, invariants, units, sentinels, thread affinity, synchronization, failure behavior, and resource ownership when declarations do not make them obvious. Public functions document observable contracts, preconditions, postconditions, nullable or sentinel results, and failure behavior where relevant.

Comments explain architectural intent and non-obvious contracts, not historical attempts or obvious syntax. External conversions name both domains and use a directional mapping. Section dividers are used only for coherent responsibilities; distinct dependencies or lifecycles require separate files. Stale comments are defects and change with the implementation. Repository-facing source and documentation do not use emoji.

## Formatting

`.clang-format` is the repository formatting authority. A structural migration does not change formatting policy incidentally. Formatting-only churn is kept separate from ownership changes, and generated or imported upstream source is not reformatted unless its owner requires it.

## Build and CMake

The live Linux preset is `linux-gcc`. Runtime and editor modules declare explicit owned source lists through `SagaModule.cmake`; programs declare only private implementation and their CMake registration. Existing aggregate targets remain during this cutover. CMake ownership changes must preserve install/export consumers and must be verified by configure, a `-j2` build, and relevant installed-consumer tests.

Nix is the preferred reproducible Linux development and validation environment, not an engine runtime dependency or a NixOS platform requirement. Durable commands use `<build-dir>` rather than treating a versioned build-output path as canonical.

## Package and publish evidence

SagaPackager owns deterministic local package validation and staging. Its `publish-check` evaluates declared package, diagnostics, script, and optional externally produced governance evidence and emits a report; it does not publish, sign, upload, install, update, or mutate source. Supplying a report proves only that local evaluation and does not establish a bundled producer for optional governance inputs.

## Public, private, and dependency boundaries

`Public/` is an intentional consumer contract; `Private/` is implementation-only. Public runtime headers do not expose vendor types. Generic editor public headers do not expose Qt; Qt-specific public spelling, when necessary, is confined to `SagaEditorQt/`. Dependencies point toward stable contracts. Server-only authority implementation does not move into generic Networking, and offline tools do not become runtime owners.

## Generated files and source truth

Generated output is never hand-edited to satisfy repository standards. Change the authoritative source, schema, template, or generator and regenerate. Generated reports are observations rather than source truth, must identify inputs and limitations, and must not silently mutate project or behavior truth.

## Testing expectations

Every ownership or public-contract change carries focused tests plus architecture or consumer checks where applicable. A passing test proves only its named scope. `Tests/Unit` remains transitional until module-local target purity; new permanent ownership should prefer module-local tests or the explicit Architecture, Integration, GPU, Contract, Support, or Evidence area.

## Repository hygiene

- Do not commit local editor, assistant, cache, private operational, or workspace state.
- CI tools resolve the repository through Git, an explicit repository-root argument, or an authoritative manifest.
- Do not add tool-specific repository-discovery markers.
- Shared Python modules belong in `Tools/common`; executable developer maintenance tools belong under `Tools/Developer`.
- License, REUSE, path-rule, and checksum metadata change with the files they govern.
