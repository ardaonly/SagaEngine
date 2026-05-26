# Phase 10A Publish Gate Reality Audit

> Last updated: 2026-05-26
> Status: Phase 10A audit complete
> Phase 10: Publish Gate / Product Packaging Reality Check

Phase 10 opens from Phase 9 Variant B limited closure. The accepted local
evidence is the normal gate pass, 36/36 with heavy labels excluded, and
`CSharpGameplayProofTests` passing inside that gate. Raw full CTest unresolved
status remains unresolved, `StressTests` and `ReplicationTests` remain heavy
opt-in debt, and no release readiness or stress/load/performance claim exists.

## Current Entry Points

Publish readiness currently enters through:

- `Saga --publish-check <project>`
- `--publish-profile <name>`, defaulting to `shipping-full`
- `--publish-report <path>`
- `--publish-diagnostics <key=path>`
- `SagaPublishReadinessService::Check`

The CLI writes `publish.report`, `publish.status`, and `publish.blockers` to
stdout/stderr and returns success only when the publish readiness result is
ready or ready with warnings.

## Current Report Behavior

The current publish report is deterministic JSON written to
`Build/Reports/publish_report.json` by default. It includes `schemaVersion`,
status, metadata, blockers, readiness, diagnostic summary, and
`packageAssetIdentityCoverage`.

The report inspects fixed client and server package manifests:

- `Build/Manifests/package_manifest.client.json`
- `Build/Manifests/package_manifest.server.json`

## Current Schema Fields

Current top-level fields:

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

`packageAssetIdentityCoverage` records package slot, package manifest existence
and load status, package id/kind, referenced asset identity manifest state,
asset manifest state, and diagnostics.

## Current Blocking Checks

Currently blocking:

- invalid or missing project manifest
- missing, malformed, or invalid package manifest
- package kind mismatch for the client/server publish slots
- missing explicit external diagnostics input
- malformed explicit external diagnostics input
- external diagnostics input with blocking diagnostics

These are deterministic and actionable. They are the only hard gate behavior
accepted by this audit.

## Current Report-Only Checks

Currently report-only through coverage diagnostics:

- asset identity manifest coverage details
- asset manifest coverage details
- missing cooked asset diagnostics
- invalid asset manifest diagnostics
- unsupported asset kind diagnostics
- duplicate asset identity mapping diagnostics

The report-only behavior is intentional for Phase 10A. It avoids treating
missing AssetPipeline cook/import, incomplete runtime/client asset consumption,
or incomplete product proof as release-blocking product failures.

## Tests That Prove Current Behavior

Focused evidence:

- `SagaPublishReadinessTests`
- `SagaPackageStagingTests`
- `RuntimePackageSmokeTests`
- `GeneratedRuntimeSmokePackageTests`
- `PackageManifestWriterTests`
- `AssetManifestWriterTests`
- `AssetIdentityManifestWriterTests`
- `AssetIdentityRuntimeContractTests`
- `RuntimeAssetBootstrapTests`
- `RuntimeAssetCatalogTests`
- `RuntimeAssetStartupBootstrapTests`

The Phase 10A smoke set passed:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R "SagaPublishReadinessTests|SagaPackageStagingTests|RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests" --output-on-failure -j 1
```

Result: 4 passed, 0 failed.

## Relationship To Phase 5

Phase 5 provides package/asset/runtime readiness foundations: package manifest
writing, asset manifest writing, identity manifest writing, generated
RuntimeSmoke manifests/packages, runtime package smoke, and runtime asset
bootstrap evidence.

Phase 10 depends on that foundation but does not complete full AssetPipeline
source discovery, import, cook, ClientHost asset consumption,
RuntimeServiceRegistry asset service, or full product packaging proof.

## Relationship To Phase 9

Phase 10 uses the Phase 9 normal gate as accepted local gate evidence:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -LE "stress|slow|load|long-running|benchmark|perf" --output-on-failure --timeout 120 -j 1
```

Raw full CTest unresolved status is warning/deferred evidence, not pass
evidence. The heavy opt-in debt remains for `StressTests` and
`ReplicationTests`.

## Warnings And Deferred Checks

Remain warning-only or deferred:

- raw full CTest unresolved
- stress/load/performance evidence not run
- heavy `ReplicationTests` not run
- full AssetPipeline cook/import missing
- ClientHost does not consume package asset catalog as product proof
- RuntimeServiceRegistry asset service missing
- full MultiplayerSandbox/product package proof missing
- broad CI hard enforcement

## Non-Goals

Phase 10A does not claim release readiness, product beta readiness, raw full
CTest health, stress/load/performance readiness, full AssetPipeline completion,
full runtime/client asset consumption, or full product packaging completion.

## Recommended Phase 10B

Proceed to Phase 10B and freeze the current report schema before expanding
report-only validation or changing any hard gate policy.
