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

`SagaGraphicsPrivate` is the private CMake implementation target shell for
future graphics backend ownership. It may depend on `SagaGraphics`, but it must
not install, publish, or directly link concrete Diligent/backend/native API
targets until a later bridge migration slice.

The internal backend preference order is documented in
[Graphics Backend Preference Order](GRAPHICS_BACKEND_PREFERENCE_ORDER.md).

## Install Surface

The development install surface currently installs
`Engine/Public/SagaEngine`. The installed `SagaEngine/Graphics` headers must
remain vendor-neutral and must not require vendored or native graphics include
paths.

The install surface must not install `Vendor/Diligent` headers under
`include/Vendor`, and must not install `VendorDiligent` or
`SagaDiligentBackend` target artifacts. Third-party license/notice files may
still be installed outside the include and target surface.

## Explicit Non-Goals

This contract does not move `SagaEngine/Render/Backend`.
It does not perform R3 bridge migration.
It does not add RenderGraph, material, shader, or resource behavior.
It does not claim `SagaEngine/Graphics` is a stable external SDK.

## Guardrails

Architecture tests must keep:

- `Graphics.h` umbrella compile smoke coverage;
- public `SagaEngine/Graphics` forbidden-token scan coverage;
- `SagaGraphics` target dependency checks;
- installed graphics include and backend/vendor target artifact scans.
