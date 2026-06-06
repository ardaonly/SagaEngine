# Graphics Target Boundary Inventory

> Last updated: 2026-06-06

This inventory records the current graphics target ownership boundary. It is a
migration guardrail, not a stable external SDK claim.

## Targets

- `SagaGraphics`: public vendor-neutral interface target for
  `Engine/Public/SagaEngine/Graphics`; exposes only `Engine/Public`.
- `SagaGraphicsPrivate`: private implementation target for graphics backend
  ownership; depends on `SagaGraphics`, may include `Engine/Private`, and may
  privately link `SagaDiligentBackend` for the R3A/R3B/R3C lifecycle,
  failure-state, conservative capability matrix v0, and R4 private slot
  registry adapter.
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

`SagaGraphics` must remain link-free and vendor-neutral. `SagaGraphicsPrivate`
may privately link `SagaDiligentBackend`, but it must not directly link
`VendorDiligent`, Diligent targets, or native graphics API targets.
`SagaGraphicsPrivate` is not installed.

## Deferred

`SagaRenderTools.cmake` and `SagaRenderTests.cmake` remain deferred until there
is a concrete render-tool target group to extract. The current R3A/R3B/R3C/R4
slices do not move `SagaEngine/Render/Backend`, do not complete the R3 bridge
migration, do not complete device-loss or swapchain recreation recovery, do not
perform native feature detection, and do not create native GPU resources.
