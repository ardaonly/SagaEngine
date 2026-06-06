# Render Public API Contract

> Last updated: 2026-06-06

This document records the current public/install render and graphics boundary.
It is a guardrail for the Diligent migration path, not a stable external SDK
claim.

## SagaGraphics Shell

`SagaEngine/Graphics` is the current vendor-neutral foundation shell for public
graphics vocabulary. It may expose copied scalar DTOs, opaque handles, and the
`Graphics.h` umbrella include used by architecture smoke tests.

`SagaGraphics` is the public CMake interface target for that shell. It exposes
only the `Engine/Public` include root. It must not link or publish:

- `VendorDiligent`
- `SagaDiligentBackend`
- Diligent targets
- native API targets or libraries for Vulkan, D3D, OpenGL, or Metal
- concrete backend libraries

`SagaGraphicsPrivate` is the private CMake implementation target for graphics
backend ownership. It may depend on `SagaGraphics` and may privately link
`SagaDiligentBackend` for the R3A-lite lifecycle adapter, but it must not
install, publish, or directly link `VendorDiligent`, Diligent targets, or native
API targets.

R3B-lite adds vendor-neutral `RenderBackendHealth` and `RenderBackendFailure`
status vocabulary to the public graphics shell so failed initialization, backend
unavailability, and minimized frame skips can be reported without exposing
backend or native graphics API types. This is a status/reporting guard, not full
device-lost recovery.

The internal backend preference order is documented in
[Graphics Backend Preference Order](GRAPHICS_BACKEND_PREFERENCE_ORDER.md).
The current graphics CMake target roles are documented in
[Graphics Target Boundary Inventory](GRAPHICS_TARGET_BOUNDARY_INVENTORY.md).

## Install Surface

The development install surface currently installs `Engine/Public/SagaEngine`,
with `SagaEngine/Graphics` delegated to `SagaInstallGraphics.cmake` as an
explicit public graphics header list. The installed graphics headers must remain
vendor-neutral and must not require vendored or native graphics include paths.

The install surface must not install `Vendor/Diligent` headers under
`include/Vendor`, and must not install `VendorDiligent` or
`SagaDiligentBackend` target artifacts. Third-party license/notice files may
still be installed outside the include and target surface.

## Explicit Non-Goals

This contract does not move `SagaEngine/Render/Backend`.
It does not complete R3 bridge migration.
It does not add RenderGraph, material, shader, or resource behavior.
It does not complete R3B device-loss or swapchain recreation recovery.
It does not claim `SagaEngine/Graphics` is a stable external SDK.

## Guardrails

Architecture tests must keep:

- `Graphics.h` umbrella compile smoke coverage;
- public `SagaEngine/Graphics` forbidden-token scan coverage;
- `SagaGraphics` target dependency checks;
- `SagaGraphicsPrivate` private target dependency checks;
- installed graphics include and backend/vendor target artifact scans.
