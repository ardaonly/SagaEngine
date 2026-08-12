# SagaEngine

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
- `LICENSING.md` — mixed-license scope and exceptions guide.

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

Built and packaged Saga runs all three managed tools through one pinned private
`Microsoft.NETCore.App` runtime. Installed Saga does not require a global
`dotnet` or .NET SDK; the wrappers resolve the private host and tool DLLs
relative to the Saga layout.

## Licensing

[LICENSE](LICENSE) is the primary MPL-2.0 license text. [LICENSING.md](LICENSING.md) explains the repository's mixed-license scope and exceptions. [LICENSE_POLICY.toml](LICENSE_POLICY.toml) is the machine-readable authority for path-specific licensing. [LICENSES/THIRD_PARTY_NOTICES.md](LICENSES/THIRD_PARTY_NOTICES.md) is the authoritative third-party notice; additional SPDX texts and validation metadata live under `LICENSES/`, `LICENSE_TEXT_SOURCES.toml`, `.reuse/dep5`, and generated checksums.

Automated GitHub test execution is temporarily disabled while its native test
graph is redesigned. Existing local test and policy commands remain available;
see the [testing roadmap](.github/TESTING_ROADMAP.md) for the repository audit,
required future coverage, and acceptance criteria for restoring CI.

## Documentation

Build and verify [SagaWiki](SagaWiki/index.html), then open it directly:

```sh
scripts/wiki build
scripts/wiki verify
```

No local server, network access, or browser-side Markdown loader is required. Edit the sources under `SagaWiki/library/`; the reader adds search, knowledge-area filters, status boundaries, and historical-source traceability. Historical audits, plans, and reports are not retained as current documentation.
