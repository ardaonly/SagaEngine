# Graphics Target Boundary Inventory

> Last updated: 2026-07-07

This inventory records the current graphics target ownership boundary. It is a
migration guardrail, not a stable external SDK claim.

## Targets

- `SagaGraphics`: public vendor-neutral interface target for
  `Engine/Public/SagaEngine/Graphics`; exposes only `Engine/Public`.
- `SagaGraphicsPrivate`: private implementation target for graphics backend
  ownership; depends on `SagaGraphics`, may include `Engine/Private`, and may
  privately link `SagaDiligentRuntime`.
- `SagaDiligentRuntime`: private target containing the concrete
  `DiligentGraphicsRuntime` implementation. It owns native device, context,
  swapchain, frame resources, native resource ownership, native binding
  execution, and command execution support.
- `SagaDiligentBackend`: temporary high-level render backend adapter over
  `DiligentGraphicsRuntime`; it must not own a separate native device/runtime
  path.
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
may privately link `SagaDiligentRuntime`, but it must not directly link
`SagaDiligentBackend`, `VendorDiligent`, Diligent targets, or native graphics
API targets. `SagaGraphicsPrivate`, `SagaDiligentRuntime`, and
`SagaDiligentBackend` are not installed.

## Deferred

`SagaRenderTools.cmake` and `SagaRenderTests.cmake` remain deferred until there
is a concrete render-tool target group to extract. `SagaForwardRenderer`,
production RenderGraph command execution, public render extension APIs,
complete device-loss recovery, and final deletion of `SagaDiligentBackend`
remain outside this boundary inventory update.
