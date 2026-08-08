---
title: Build, package, and publish toolchain
status: guide
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Build, package, and publish toolchain

Tool ownership is intentionally split. SagaTools dispatches registered tools without absorbing their behavior, Forge prepares dependencies and profiles, CMake declares build targets, focused developer checks enforce repository contracts, AssetPipeline produces deterministic Runtime-compatible manifests, SagaProjectKit validates project manifests, SagaScript owns script workflows, and SagaPackager stages package outputs.

## Dispatcher boundary

`Tools/SagaTools` provides the `tools` dispatcher and a flat registry. It can list, locate, install through a registered bootstrap, or launch an owning tool while forwarding arguments. It is not an orchestrator or build system. Removing the dispatcher must not make the underlying tools impossible to invoke directly. Its detailed setup and registry discovery rules remain in `Tools/SagaTools/README.md`.

## AssetPipeline boundary

`SagaAssetPipelineLib` allocates stable asset identifiers and writes identity, asset, and package manifests accepted by current Runtime loaders. It does not perform arbitrary source import/cooking or package staging. See [Asset pipeline and manifest generation](../reference/asset-pipeline-and-manifest-generation.md).

## Build ownership

### Snapshot-only project builds

The Qt-free `StandaloneBuildCoordinator` is the shared project-build boundary for the Editor and the `SagaStandaloneBuild` command-line entry. Capture requires saved source, validates the current package generation, inventories and hashes project inputs, binds the selected validated BuildProfiles entry and private tool/runtime identities, and checks the workspace and package identities again before accepting the snapshot.

Compilation consumes one materialized snapshot through the existing SagaProjectKit, SagaScript, SagaPackager, and Play preparation owners. A unique owned staging directory, explicit publish journal, candidate directory, and recoverable previous-output rename protect the last valid output. Root, repository, project-source, collision, and symlink output paths are rejected before mutation. Cancellation or verification failure does not publish a candidate.

`StandaloneManifest.saga.json` separates publication-time metadata from the stable content identity. Reproducibility is classified only after comparing two outputs from the same snapshot. A content-addressed semantic match is not reported as byte reproducibility when local envelopes, publication paths, or other artifacts differ.

This capability builds a verified local standalone project artifact. It does not claim platform signing, installers, cross-machine reproducibility, console targets, or public distribution readiness.

The live Linux preset is `linux-gcc`. Source lists and link visibility belong to the owning module `CMakeLists.txt`; top-level files should compose modules rather than absorb implementation ownership.

```sh
nix-shell --run "Tools/Forge/bin/forge install --profile linux-gcc"
nix-shell --run "cmake --preset linux-gcc"
nix-shell --run "cmake --build --preset linux-gcc"
```

Generated build directories are disposable output. They must not become inputs to source review or Wiki generation.

## Package boundary

SagaPackager consumes validated project/package inputs and can accept optional externally produced governance evidence. It does not create that governance report, and no bundled policy kit should be inferred. Package staging must preserve source identity and fail clearly on invalid or missing required evidence.

The only project manifest input is `.sagaproj` schema version `0`; legacy manifest filenames are rejected rather than discovered as fallbacks. `sagapack stage` and `sagapack publish-check` are the canonical staging and readiness commands. Launcher does not own alternate `--stage-packages` or `--publish-check` workflows.

## Publish boundary

Publishing is a gated operation over a known package and evidence set. Historical publish documents provide durable lessons about explicit inputs, dry runs, and immutable results, but they are not proof of a currently shipped distribution service. Current commands and profiles in the repository take precedence over old examples.

## Linux distribution evidence chain

The current Linux path separates four results: a staged distribution layout, package preflight, archive/checksum generation, and smoke verification from an unpacked archive. Preflight records required, available, and missing inputs; it is not the package itself. The smoke verifies checksum, archive root/layout, required executable bits, limited help commands, and named sample/tool workflows. Each stage records limitations, and a successful checksum proves integrity of those bytes—not runtime, editor, server, or release readiness.

The staged tool surface contains launchers for `sagaproject`, `sagascript`, and
`sagapack`, their published DLL/runtime metadata, and one Saga-owned
`Microsoft.NETCore.App` 10.0.10 runtime under `runtime/dotnet`. CMake acquires
the pinned `linux-x64` runtime archive only while preparing the build/package
stage and verifies its SHA-512 before traversal-safe extraction. Product
execution performs no download and requires no machine-global .NET installation.
Python packaging copies this CMake-owned stage instead of publishing or
discovering tools independently.

Developer-tree and distribution wrappers both launch the private host plus the
tool DLL with multilevel lookup disabled. Installed execution ignores PATH,
`SAGA_DOTNET`, and conflicting `DOTNET_ROOT` values. SDK, ASP.NET, source,
managed apphosts, and unrelated executable payloads are forbidden from the
distribution.

## Coding and repository hygiene

Keep public/private includes consistent with ownership, use formatters for mechanical style, and keep generated or local audit output out of commits. A wrapper should route to the owning tool, not grow a second implementation.

## Detailed references

- [Build, package, and distribution contracts](../reference/build-package-and-distribution-contracts.md)
- [Engineering conventions and contract review](../reference/engineering-conventions-and-review.md)
- [Licensing, contribution, and third-party governance](../reference/licensing-contribution-and-third-party.md)
