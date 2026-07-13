# Assets And Packages

> Last updated: 2026-07-13

Asset and package support exists for selected local workflows. It is not a full
asset import/cook pipeline or release packaging system.

## Runtime Consumption Boundary

Runtime consumes runtime-ready assets and manifests. Asset import, cook,
package staging, publish reports, stale artifact analysis, and editor asset UX
belong to their own tools and contracts.

The runtime asset path may validate manifests and load supported runtime
artifacts. It must not be described as owning the full source asset pipeline or
release packaging workflow.

## Current State

- Package manifest, asset manifest, asset identity, RuntimeSmoke package, and
  runtime asset bootstrap support exist.
- Package checks can report package/asset/identity coverage.
- Full AssetPipeline source discovery/import/cook is incomplete.
- Complete runtime asset consumption is incomplete.
- RuntimeServiceRegistry asset service is incomplete.
- A playable packaged MultiplayerSandbox path is missing.

## Evidence Boundary

Current evidence supports selected package manifests, asset manifests, asset
identity reporting, and runtime asset bootstrap paths. It does not prove a full
asset import/cook pipeline, complete package distribution, or a playable
packaged sample.
