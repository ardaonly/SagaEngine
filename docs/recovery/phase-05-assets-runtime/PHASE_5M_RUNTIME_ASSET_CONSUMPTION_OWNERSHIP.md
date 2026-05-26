# Phase 5M Runtime Asset Consumption Ownership

> Last updated: 2026-05-25
> Status: Phase 5M closed as Runtime asset consumption ownership audit
> Decision: reject raw `AssetRegistry` handoff to `ClientHost`; add a Runtime read-only asset access contract next

Phase 5M closes as an ownership audit and Phase 5 closure direction
checkpoint. It records how package-bootstrapped assets should become
consumable without forcing `ClientHost`, render, UI, network, or service
lifecycle ownership changes.

Full CTest remains unverified.

## Closure Summary

Phase 5M is accepted as:

- Runtime asset consumption ownership audit
- ClientHost raw `AssetRegistry` rejection
- Runtime read-only asset access contract recommendation
- Phase 5 closure direction checkpoint

Phase 5M is not accepted as:

- Runtime asset access facade implementation
- ClientHost asset consumption
- RuntimeServiceRegistry asset service
- publish readiness report/hard gate
- source discovery/import/cook
- UI/document `AssetKind`
- MultiplayerSandbox package readiness
- full AssetPipeline completion

## Evidence

- Apps/Runtime can bootstrap explicit package assets into a local
  `AssetRegistry` during startup.
- Apps/Runtime creates the local registry before `ClientHost` construction and
  intentionally does not pass it into `ClientHost`.
- `ClientHost` currently owns mixed network, input, UI, debug render, and
  process-session setup. It has no narrow asset ownership boundary.
- `RuntimeAssetBootstrap` is used by the runtime executable path through the
  Apps/Runtime helper.
- `RuntimeServiceRegistry` has service registration and lifecycle ordering, but
  service `Start()` has no startup context for package manifests, registries, or
  asset lifetimes.
- Existing resource abstractions (`ResourceManager`, `StreamingManager`,
  `IAssetSource`, file and virtual-file asset sources) consume runtime asset
  ids and sources, not package ownership.
- Full CTest remains unverified.

## Ownership Decision

`ClientHost` should not receive raw `AssetRegistry` now. Passing the registry
directly would make `ClientHost` the accidental owner of package asset lifetime
while it is still coupled to rendering, UI, networking, and session startup.

Runtime should expose a narrow read-only access boundary over `AssetRegistry`.
That boundary can later be passed to runtime systems or adapted into a service
without giving consumers registry mutation APIs.

`RuntimeServiceRegistry` should not own an asset service yet. A service shell
needs a startup context and lifetime contract first: package manifest path,
package base directory, bootstrap result, registry storage, and shutdown
ownership.

Apps/Runtime's local `AssetRegistry` remains a temporary startup proof and
lifetime placeholder until the Runtime facade or a later Runtime service owns
the consumption boundary.

Publish readiness report v0 remains valuable, but should follow the Runtime
asset access boundary so the report can describe a real runtime consumption
contract instead of only package staging.

## Inventory

Current `AssetRegistry` use:

- Engine resource tests and runtime fixture tests populate and query registry
  entries by `AssetId`, `AssetKey`, and `AssetKind`.
- `RuntimeAssetRegistryBootstrapper` registers package manifest assets into a
  caller-owned registry.
- `RuntimeAssetBootstrap` validates packages and delegates registry population
  without taking ownership.
- Apps/Runtime owns a local registry during startup and does not publish it to
  `ClientHost`.

Current `RuntimeAssetBootstrap` use:

- Focused Runtime tests prove successful RuntimeSmoke registration and failure
  without partial mutation.
- Apps/Runtime helper uses it for explicit package startup.

Current `ClientHost` asset/resource/UI use:

- `ClientConfig` carries UI content root and process/session options.
- `ClientHost` initializes network session, input, debug rendering, and Runtime
  UI bootstrap.
- It does not include or query `AssetRegistry`, `RuntimeAssetBootstrap`, or
  package asset manifests.

Supported package asset kinds today:

- Runtime registry kinds support mesh, texture, shader, audio, animation, and
  material assets.
- RuntimeSmoke proves a texture asset path.
- UI/document assets do not have a runtime package `AssetKind` yet.

Tests proving package/runtime asset readiness:

- `RuntimePackageSmokeTests`
- `GeneratedRuntimeSmokePackageTests`
- `SagaPackageStagingTests`
- `RuntimeAssetBootstrapTests`
- `RuntimeAssetStartupBootstrapTests`

Remaining broad or unverified test areas:

- Full CTest is unverified.
- Registered-but-unbuilt target warnings remain in the current build inventory.
- ClientHost/runtime asset consumption has no integration test yet.

## Phase 5N Recommendation

Pick Phase 5N: Runtime Asset Access Facade.

The first safe implementation after Phase 5M is a Runtime-owned read-only
catalog over caller-owned `AssetRegistry`. It should prove lookup semantics by
`AssetKey`, `AssetId`, and available registry metadata without `ClientHost`
wiring.

Phase 5 can move toward closure after this facade and a report-only publish
readiness relationship are documented. It still should not close before the
remaining package/runtime consumption and publish-readiness debt is explicit.
