---
title: Asset pipeline and manifest generation
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Asset pipeline and manifest generation

`Tools/AssetPipeline` is the current C++ owner for deterministic asset identity allocation and for writing Runtime-compatible identity, asset, and package manifests. It builds as `SagaAssetPipelineLib` and is exercised by focused unit and contract tests. It is a library surface, not yet a complete importer, cooker, editor UX, or command-line product.

## Boundary with Runtime

The pipeline produces records; Runtime modules consume and validate them. Shared compatibility is proven through the checked-in Runtime loaders and startup gates rather than through a separate copied schema.

| Pipeline operation | Runtime consumer boundary |
| --- | --- |
| Allocate `AssetKey` to `AssetId` mappings | Asset identity manifest loader and resolver |
| Write an identity manifest | Runtime identity schema and registry bootstrap |
| Write asset entries | Asset manifest loader and file validation |
| Write package metadata and manifest references | Package startup validation |

The generator does not mount packages, stream bytes, create GPU resources, or mutate a live registry. The Runtime does not invent stable identities by scanning whatever files are present at startup.

## Stable asset identity

`AssetIdentityGenerator` accepts the current asset keys and a previous valid mapping set. Existing keys retain their identifiers. New keys are sorted lexicographically and allocated after the highest previous identifier, making the result independent of input enumeration order.

Deleted identifiers are not reused by the current policy. A rename is therefore a new identity unless an explicit migration is introduced. This protects references from silently resolving to unrelated content after deletion and later creation.

Asset identifier zero is invalid. Duplicate current keys, duplicate previous keys or identifiers, invalid previous identifiers, and allocation overflow produce structured failure. Failure does not emit a partially valid mapping.

These rules give deterministic identity continuity for the supplied history. They do not solve semantic rename detection, distributed concurrent authoring, or long-term schema migration by themselves.

## Writers are validate-then-write

The identity, asset, and package writers validate their complete input before creating the requested artifact. Their results carry stable diagnostic identifiers for invalid values and I/O failures.

Identity-manifest writing checks key and identifier validity and rejects duplicates. Asset-manifest writing checks identifiers, recognized asset kinds, safe paths, duplicate assets, and dependency validity. Package-manifest writing checks package identity/kind, build profile, target platform, Runtime compatibility, referenced-manifest identifiers, duplicate references, and safe reference paths.

Unsafe paths are rejected at the artifact boundary. A path in a manifest is not permission to escape the output/project/package root. The writer must not normalize traversal into an apparently valid external file.

## Writers do not manufacture dependencies

`PackageManifestWriter` records references to identity and asset manifests; it does not generate those referenced manifests as a side effect. Callers must run the owning operations in a visible order and pass the resulting paths. This keeps a package-manifest failure from hiding which earlier artifact was absent or stale.

Likewise, generating a JSON manifest does not prove that all referenced payloads exist, have the declared bytes, or can be loaded by every Runtime workflow. Startup validation and package tests provide the next boundary.

## Current evidence

Focused tests currently establish the following bounded claims:

- existing mappings are reused and new identifiers are deterministic;
- deleted identifiers are not reused and renames become new identity;
- duplicate, invalid, and overflow inputs fail;
- writer output carries the expected Runtime schema version and loads through Runtime readers;
- generated identity and asset manifests can bootstrap the Runtime registry fixture;
- generated package inputs pass the current startup gate;
- missing package or referenced manifests and unsafe references fail;
- invalid inputs do not leave an artifact presented as successful;
- a generated RuntimeSmoke package can replace hand-authored fixture manifests in its focused path.

This evidence is stronger than a documentation-only schema claim because producer and consumer execute against the same fixtures. It remains narrower than complete content cooking or distribution readiness.

## Relationship to other tools

SagaProjectKit validates the canonical `.sagaproj` contract and project boundaries. SagaScript produces script-specific artifacts and manifests. SagaPackager stages validated package inputs and optional external governance evidence. Forge and CMake own dependency/build orchestration. AssetPipeline must not duplicate those responsibilities.

A future command-line frontend may call `SagaAssetPipelineLib`, but the library should remain the contract owner for deterministic allocation and serialization. A wrapper should report its inputs, outputs, and diagnostics rather than silently discovering source trees or choosing identities.

## Source and generated-output rules

Asset source and authoring metadata remain project-owned source. Identity, asset, and package manifests produced by the pipeline are derived artifacts unless a project contract explicitly designates one as maintained source. Generated output should live under the selected output root and remain reproducible from declared inputs.

Do not write generated manifests into sample source directories merely to make a local smoke pass. Tests should use temporary output roots, validate the result, and leave the source tree unchanged.

For Runtime consumption see [Runtime lifecycle, assets, and packages](runtime-lifecycle-assets-and-packages.md). For registry and residency behavior see [Resource streaming and residency](resource-streaming-and-residency.md).

