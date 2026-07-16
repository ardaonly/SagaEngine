# SagaEngine

[![SagaEngine CI](https://github.com/ardaonly/SagaEngine/actions/workflows/build.yml/badge.svg)](https://github.com/ardaonly/SagaEngine/actions/workflows/build.yml)
[![Licensing Policy](https://github.com/ardaonly/SagaEngine/actions/workflows/licensing.yml/badge.svg)](https://github.com/ardaonly/SagaEngine/actions/workflows/licensing.yml)
[![Heavy Test Evidence](https://github.com/ardaonly/SagaEngine/actions/workflows/heavy-evidence.yml/badge.svg)](https://github.com/ardaonly/SagaEngine/actions/workflows/heavy-evidence.yml)

SagaEngine is an active open-source game-engine and authoring-toolchain codebase focused on explicit module ownership, authoritative simulation, project/package contracts, C# source authoring, rendering foundations, editor infrastructure, and evidence-bounded development.

It is not a finished game-engine product, public SDK, production networking stack, or end-to-end editor workflow. See [SagaWiki](SagaWiki/index.html) for the canonical human-facing documentation and [capability boundaries](SagaWiki/index.html#non-claims) before interpreting repository foundations as product completion.

## Repository layout

- `Engine/Source/Runtime` — runtime modules with explicit Public, Private, Tests, and CMake ownership.
- `Engine/Source/Editor` — editor core/framework, Qt, authoring, Visual Blocks, scripting, collaboration, and experimental modules.
- `Engine/Source/Programs` — thin program entry points and program-specific integration.
- `Tools` — Forge, project, scripting, packaging, licensing, and developer tooling.
- `Tests` — unit, architecture, integration, GPU, contract, support, evidence, and fixture areas.
- `Samples` — sample-owned projects and source.
- `SagaWiki` — canonical current product and architecture documentation.
- `LICENSES` — SPDX license texts and authoritative third-party notices.

The legacy top-level source roots are retired. The current map is in [Repository map](SagaWiki/index.html#repository-map), and dependency rules are in [Ownership and dependency direction](SagaWiki/index.html#ownership-dependencies).

## Build

The live Linux preset is `linux-gcc`. Forge owns dependency setup; CMake owns configure and build:

```sh
nix-shell --run "Tools/Forge/bin/forge install --profile linux-gcc"
nix-shell --run "cmake --preset linux-gcc"
nix-shell --run "cmake --build --preset linux-gcc"
ctest --test-dir build/RelWithDebInfo-0.0.11 --output-on-failure
```

The stable root wrappers are:

```sh
scripts/build
scripts/test
scripts/verify
scripts/package
scripts/wiki build
scripts/wiki verify
```

The wiki wrapper owns standard-library-only documentation generation and verification. The other wrappers delegate to Forge, the owning verification tools, or SagaPackager and contain no product logic.

## Public tooling direction

The intentionally small public CLI surface is:

- `sagaproject` — `.sagaproj` manifest validation and resolution;
- `sagascript` — C# analysis, compilation, and source-authoring workflows;
- `sagapack` — package/profile inspection and staging.

SagaTools remains the stable dispatcher. Developer checks below `Tools/Developer` are internal unless SagaWiki explicitly promotes them.

## Licensing

[LICENSE](LICENSE) is the primary repository license. [LICENSES/THIRD_PARTY_NOTICES.md](LICENSES/THIRD_PARTY_NOTICES.md) is the authoritative third-party notice. Additional SPDX texts live under `LICENSES/`; machine-checked policy lives in `LICENSE_POLICY.toml`, `LICENSE_TEXT_SOURCES.toml`, `.reuse/dep5`, and generated checksums.

Pull requests run the strict policy and DCO gate. Relevant source/build/licensing changes also run the configured CMake ownership graph; its JSON report and installed-consumer evidence remain visible as workflow artifacts. See [.github/MAINTENANCE.md](.github/MAINTENANCE.md) for required checks and retention.

## Documentation

Build and verify [SagaWiki](SagaWiki/index.html), then open it directly:

```sh
scripts/wiki build
scripts/wiki verify
```

No local server, network access, or browser-side Markdown loader is required. Edit the sources under `SagaWiki/library/`; the reader adds search, knowledge-area filters, status boundaries, and historical-source traceability. Historical audits, plans, and reports are not retained as current documentation.
