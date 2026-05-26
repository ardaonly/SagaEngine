# Phase 10B Publish Report Schema Freeze

> Last updated: 2026-05-26
> Status: Phase 10B schema freeze complete
> Phase 10: Publish Gate / Product Packaging Reality Check

Phase 10B freezes the current publish report as schema version 1. This is a
report contract freeze, not a broad publish report rewrite and not a new hard
gate.

## Phase 10A Recap

Phase 10A found that publish readiness already has a product entry point,
deterministic report output, existing blockers for project/package/explicit
diagnostics failures, and report-only package/asset/identity coverage through
`packageAssetIdentityCoverage`.

## Schema Contract

The stable schema version is:

```json
{ "schemaVersion": 1 }
```

Stable required top-level fields:

- `schemaVersion`
- `reportId`
- `buildId`
- `packageId`
- `status`
- `diagnosticSummary`
- `metadata`
- `blockers`
- `readiness`
- `packageAssetIdentityCoverage`

Stable `readiness` fields:

- `status`
- `blockers`

Stable blocker fields:

- `kind`
- `message`
- `suggestedAction`
- `diagnostics`
- `metadata`

Stable package coverage fields:

- `packageSlot`
- `packageManifestPath`
- `packageManifestExists`
- `packageManifestLoads`
- `packageId`
- `packageKind`
- `assetIdentityManifest`
- `assetManifests`
- `diagnostics`

Stable asset identity coverage fields:

- `referenced`
- `path`
- `resolvedPath`
- `exists`
- `loads`
- `mappingCount`
- `diagnostics`

Stable asset manifest coverage fields:

- `id`
- `path`
- `resolvedPath`
- `exists`
- `loads`
- `assetCount`
- `diagnostics`

## Report-Only Diagnostic Fields

Coverage diagnostics are stable report-only diagnostic fields:

- `code`
- `message`
- `path`
- `referenceIndex`
- `assetIndex`
- `mappingIndex`
- `field`

They describe package, asset manifest, and identity manifest coverage without
automatically becoming hard blockers.

## Unstable Or Deferred Fields

Deferred fields:

- explicit raw full CTest status ingestion
- explicit heavy gate status ingestion
- source cook/import diagnostics
- ClientHost package consumption status
- RuntimeServiceRegistry asset service status
- full product package proof status
- CI enforcement state

These fields need later schema versioning or a documented extension before they
can become stable report contract.

## Fields That Must Not Become Blocking Yet

The following must remain warning/report-only until later evidence exists:

- missing cooked asset coverage
- unsupported asset kind coverage
- duplicate asset identity mapping coverage
- raw full CTest unresolved
- heavy opt-in gate unresolved
- missing source cook/import pipeline
- missing RuntimeServiceRegistry asset service
- missing ClientHost asset consumption proof

## Test Evidence

`SagaPublishReadinessTests` now asserts parsed JSON schema shape, schema
version 1, stable top-level fields, stable readiness/blocker arrays, and stable
package coverage fields.

Focused verification:

```sh
Tools/Forge/bin/forge nix build --target SagaPublishReadinessTests --build=build/RelWithDebInfo-0.0.9 --jobs=1
ctest --test-dir build/RelWithDebInfo-0.0.9 -R SagaPublishReadinessTests --output-on-failure -j 1
```

Result: `SagaPublishReadinessTests` passed.

## Non-Goals

Phase 10B does not add a broad profile system, new package system, CI hard
gate, source cook/import, runtime/client asset consumption, or raw full CTest
claim.

## Phase 10C Decision

Phase 10C report-only expansion is safe because the schema is stable enough for
focused coverage diagnostics and the existing test seam supports parsed JSON
assertions.

