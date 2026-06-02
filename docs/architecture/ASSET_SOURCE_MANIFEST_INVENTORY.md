# Asset Source Manifest Inventory

## Status

Phase 134 report-only contract for Hedef 4. This document defines how
SagaProjectKit inventories asset roots and asset source manifests without
importing, cooking, packaging, launching, or mutating project source.

## Canonical Inputs

Project `.sagaproj` asset entries declare asset roots. A root is accepted as a
source asset root when it exists under the project and contains source-controlled
asset metadata named `assets.source.json` or `asset_manifest.json`.

An asset root that exists without one of those metadata files is reported as a
placeholder asset root. Missing roots remain manifest reference failures.

`assets.source.json` and asset-root `asset_manifest.json` files may declare:

- `manifestId`
- `canonical`
- `assetOwners`
- `assets`
- `assetReferences`

Each asset owner entry must declare a stable `assetId`, project-relative `path`,
and `owner`.

## Non-Canonical Evidence

Asset manifests discovered under generated, build, or package output locations
are classified as non-canonical evidence. They may be listed in reports, but
they cannot become the owner of an asset reference.

Generated projections, package manifests, diagnostics, and build outputs remain
evidence snapshots. They are not authoring source.

## Report

`sagaproject asset-source-manifest-inventory` emits
`asset_source_manifest_inventory_report.json` with:

- `assetRoots`
- `assetSourceManifests`
- `assetOwners`
- `assetReferences`
- `missingOwners`
- `generatedArtifacts`
- `diagnostics`
- `localOnly=true`
- `networkExposure="None"`
- `mutatesSource=false`
- `enforcement="ReportOnly"`
