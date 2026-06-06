# Graphics Target Boundary Inventory

> Last updated: 2026-06-06

This inventory records the current graphics target ownership boundary. It is a
migration guardrail, not a stable external SDK claim.

## Targets

- `SagaGraphics`: public vendor-neutral interface target for
  `Engine/Public/SagaEngine/Graphics`; exposes only `Engine/Public`.
- `SagaGraphicsPrivate`: private implementation target shell for future
  graphics backend ownership; depends on `SagaGraphics` and may include
  `Engine/Private`.
- `SagaDiligentBackend`: concrete render backend target that owns the current
  Diligent implementation path.
- `VendorDiligent`: private vendor aggregation target for vendored Diligent
  targets and native graphics dependencies.

## Module Boundaries

- `cmake/modules/SagaGraphicsTargets.cmake` owns the `SagaGraphics` and
  `SagaGraphicsPrivate` target definitions.
- `cmake/modules/SagaDiligentVendor.cmake` owns `VendorDiligent` and Diligent
  link helper setup.
- `cmake/modules/SagaInstallGraphics.cmake` owns the explicit
  `SagaEngine/Graphics` public header install rule.

`SagaGraphics` and `SagaGraphicsPrivate` must not directly link
`VendorDiligent`, `SagaDiligentBackend`, Diligent targets, or native graphics
API targets. `SagaGraphicsPrivate` is not installed.

## Deferred

`SagaRenderTools.cmake` and `SagaRenderTests.cmake` remain deferred until there
is a concrete render-tool target group to extract. The current slice does not
move `SagaEngine/Render/Backend` and does not perform the R3 bridge migration.
