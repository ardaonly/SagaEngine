# SagaEngine

SagaEngine is an active open-source game-engine and authoring-toolchain codebase focused on explicit module ownership, authoritative simulation, project/package contracts, C# source authoring, rendering foundations, editor infrastructure, and evidence-bounded development.

It is not a finished game-engine product, public SDK, production networking stack, or end-to-end editor workflow. See [SagaWiki](SagaWiki/index.html) for the canonical human-facing documentation and [capability boundaries](SagaWiki/index.html#non-claims) before interpreting repository foundations as product completion.

## Repository layout

- `Engine/Source/Runtime` — runtime modules with explicit Public, Private, Tests, and CMake ownership.
- `Engine/Source/Editor` — editor core/framework, Qt, authoring, Visual Blocks, scripting, collaboration, and experimental modules.
- `Engine/Source/Programs` — thin program entry points and program-specific integration.
- `Tools` — project, scripting, packaging, licensing, and developer tooling; [Forge](Tools/Experimental/Forge/README.md) is optional and experimental.
- `Tests` — unit, architecture, integration, GPU, contract, support, evidence, and fixture areas.
- `Samples` — sample-owned projects and source.
- `SagaWiki` — canonical current product and architecture documentation.
- `LICENSES` — SPDX license texts and authoritative third-party notices.
- `LICENSING.md` — mixed-license scope and exceptions guide.

The legacy top-level source roots are retired. The current map is in [Repository map](SagaWiki/index.html#repository-map), and dependency rules are in [Ownership and dependency direction](SagaWiki/index.html#ownership-dependencies).

## Build

The primary Linux build uses Conan for third-party dependencies and CMake for configuration and compilation. Forge is experimental and is not required to build SagaEngine.

```sh
git submodule update --init --recursive
conan install . --profile:host=profiles/linux-gcc --build=missing
export SAGA_ENGINE_VERSION="$(tr -d '[:space:]' < VERSION)"
cmake --preset linux-gcc
cmake --build --preset linux-gcc --parallel 2
```

This path expects a GCC-compatible compiler, CMake, Conan 2, Ninja, sccache,
and the host development libraries used by the enabled Linux graphics/window
backends.

NixOS users may enter the optional development shell with `nix-shell` and run
the same Conan/CMake steps there. The shell supplies NixOS-specific tool and
library compatibility; it is not the project's only Linux build path.

The stable root wrappers are:

```sh
scripts/build
scripts/test
scripts/verify
scripts/package
scripts/wiki build
scripts/wiki verify
```

The wiki wrapper owns standard-library-only documentation generation and verification. The other wrappers route to CMake, CTest, the owning verification tools, or SagaPackager and contain no product logic.

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
