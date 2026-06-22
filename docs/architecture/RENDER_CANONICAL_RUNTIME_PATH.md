# Render Canonical Runtime Path

> Last updated: 2026-06-22

This document records the current short-term runtime proof path:

```text
IRenderBackend -> DiligentRenderBackend -> Vulkan
```

It is not a stable SDK claim and it is not a production-renderer claim. The
purpose is to define what has been proven by automated GPU tests and what still
requires later renderer work.

## Current Verified Claim

The highest automated claim is:

```text
PIXEL-CORRECT CUBE VERIFIED
```

Meaning:

- a real Diligent/Vulkan swapchain can be initialized from the render backend;
- the current color backbuffer can be copied to CPU-visible staging memory
  before present;
- captured backbuffer data is normalized to RGBA8 for tests;
- indexed mesh submission produces non-clear pixels;
- uploaded albedo textures affect rendered pixels;
- depth testing keeps nearer geometry visible over farther geometry;
- model transforms change screen-space coverage;
- resize and multi-frame rendering continue to produce valid pixels;
- destroyed mesh/material handles are rejected by submission diagnostics.

The next manual claim is:

```text
BASIC PLAYABLE 3D SLICE VERIFIED
```

That claim requires the Sandbox scenario `first_3d_vertical_slice` to be
launched and observed interactively. A successful build alone is not enough.

## Capture Boundary

`DiligentRenderBackend::CaptureCurrentColorFrame()` is a private/concrete
diagnostic API. It is intentionally not part of `IRenderBackend`, `SagaGraphics`,
or the installed public graphics surface.

Capture happens after `BeginFrame()` and any `Submit()` calls, but before
`EndFrame()` / `Present()`. This is the reliable point at which the current
backbuffer still represents the frame being tested.

The capture implementation copies the current swapchain backbuffer texture into
a staging texture, waits for the immediate context, maps the staging texture,
and normalizes supported RGBA8/BGRA8 UNORM or SRGB backbuffer formats into
`RenderPixelFormat::RGBA8_UNORM`.

## Test Mapping

| Test | Evidence |
| --- | --- |
| `DiligentGPU.ClearFrameCanBeReadBack` | Swapchain color backbuffer can be copied and read before present. |
| `DiligentGPU.IndexedCubeSubmitsAndPresents` | M1 draw-call diagnostics still report indexed draw and present. |
| `DiligentGPU.IndexedCubeProducesNonClearPixels` | Indexed cube produces captured non-clear pixels, not just counters. |
| `DiligentGPU.UploadedTextureAffectsRenderedPixels` | Uploaded RGBA texture affects rendered output. |
| `DiligentGPU.NearGeometryOccludesFarGeometry` | Depth testing is active for opaque draws. |
| `DiligentGPU.ModelTransformChangesScreenCoverage` | Per-draw model transform reaches shader-visible screen coverage. |
| `DiligentGPU.IndexedCubeRendersAcrossMultipleFrames` | Diagnostics reset per frame and repeated captures remain valid. |
| `DiligentGPU.ResizeContinuesProducingValidPixels` | Swapchain resize invalidates stale capture staging and preserves rendering. |
| `DiligentGPU.DestroyedResourcesAreRejectedBySubmission` | Destroyed mesh/material handles are rejected without drawing stale resources. |

## Sandbox Slice

The runtime scenario id is:

```text
first_3d_vertical_slice
```

Manual launch command:

```bash
nix-shell --run "<build-dir>/bin/SagaSandbox first_3d_vertical_slice --vulkan"
```

Use the active configured build directory, for example the current
`build/RelWithDebInfo-*` directory. Durable architecture docs should not pin
changing `0.0.x` build folder names. See
[NIX_DEVELOPMENT_AND_VALIDATION_POLICY.md](NIX_DEVELOPMENT_AND_VALIDATION_POLICY.md).

The scenario owns its resources in `OnInit()` and destroys them in
`OnShutdown()`. It creates a ground plane, multiple textured cubes, perspective
camera output, depth-tested opaque materials, and a rotating cube. Movement uses
WASD with mouse look through the existing Sandbox input stack.

## Explicit Non-Claims

This slice does not complete R4, R4B, or R5.

It does not prove:

- native SagaGraphics resource upload;
- native descriptor set/ring-buffer/frame-resource integration;
- RenderGraph-to-Diligent execution;
- asset-driven scene loading;
- material graph authoring;
- lighting, shadows, post-processing, terrain, water, VFX, or character
  rendering completion;
- production renderer readiness.
