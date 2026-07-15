# SagaEngine

SagaEngine is an active open-source game-engine and authoring-toolchain codebase focused on explicit module ownership, authoritative simulation, project/package contracts, C# source authoring, rendering foundations, editor infrastructure, and evidence-bounded development.

It is not a finished game-engine product, public SDK, production networking stack, or end-to-end editor workflow. See [SagaWiki](SagaWiki/index.html) for the canonical human-facing documentation and [not-implemented boundaries](SagaWiki/pages/not-implemented.html) before interpreting repository foundations as product completion.

## Repository layout

- `Engine/Source/Runtime` — runtime modules with explicit Public, Private, Tests, and CMake ownership.
- `Engine/Source/Editor` — editor core/framework, Qt, authoring, Visual Blocks, scripting, collaboration, and experimental modules.
- `Engine/Source/Programs` — thin program entry points and program-specific integration.
- `Tools` — Forge, project, scripting, packaging, licensing, and developer tooling.
- `Tests` — unit, architecture, integration, GPU, contract, support, evidence, and fixture areas.
- `Samples` — sample-owned projects and source.
- `SagaWiki` — canonical current product and architecture documentation.
- `LICENSES` — SPDX license texts and authoritative third-party notices.

The legacy top-level source roots are retired. The detailed ownership table is in [Repository layout](SagaWiki/pages/repository-layout.html) and the dependency rules are in [Module boundaries](SagaWiki/pages/module-boundaries.html).

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
scripts/wiki 8000
```

They delegate to Forge, the owning verification tools, SagaPackager, or the local SagaWiki HTTP server. They contain no product logic.

## Public tooling direction

The intentionally small public CLI surface is:

- `sagaproject` — `.sagaproj` manifest validation and resolution;
- `sagascript` — C# analysis, compilation, and source-authoring workflows;
- `sagapack` — package/profile inspection and staging.

SagaTools remains the stable dispatcher. Developer checks below `Tools/Developer` are internal unless SagaWiki explicitly promotes them.

## Licensing

[LICENSE](LICENSE) is the primary repository license. [LICENSES/THIRD_PARTY_NOTICES.md](LICENSES/THIRD_PARTY_NOTICES.md) is the authoritative third-party notice. Additional SPDX texts live under `LICENSES/`; machine-checked policy lives in `LICENSE_POLICY.toml`, `LICENSE_TEXT_SOURCES.toml`, `.reuse/dep5`, and generated checksums.

## Documentation

Open [SagaWiki](SagaWiki/index.html) directly, or run:

```sh
scripts/wiki 8000
```

Then visit `http://localhost:8000`. Historical audits, plans, and reports are not retained as current documentation.
