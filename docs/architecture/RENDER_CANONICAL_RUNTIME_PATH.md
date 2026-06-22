# Render Canonical Runtime Path

> Last updated: 2026-06-22

This document records the current short-term runtime proof path:

```text
IRenderBackend -> DiligentRenderBackend -> Vulkan
```

It is not a stable SDK claim and it is not a production-renderer claim. The
purpose is to define what has been proven by automated GPU tests and what still
requires later renderer work.

GFX-M3 did not change the `IRenderBackend` virtual contract. It did add public
render vocabulary to `RenderView` and `MaterialRuntime`: lighting data and
`OpaqueShadingModel`. These installed public header changes are source-visible
and can affect ABI where those structs cross binary boundaries.

## Current Verified Claim

The highest automated claim is:

```text
SHADOW OCCLUSION PIXEL-VERIFIED
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
- directional light data changes captured pixel luminance;
- vertex normals affect GPU lighting output;
- ambient contribution remains separate from directional light;
- unlit materials ignore directional light and shadow sampling;
- a persistent directional shadow map is populated by a depth pass;
- lit materials sample that shadow map in the main pass;
- projected receiver shadows are verified by pixel readback;
- shadow toggling, multi-frame rendering, and resize preserve the shadow path.

The next manual claim is:

```text
BASIC LIT PLAYABLE 3D SLICE VERIFIED
```

That claim requires the Sandbox scenario `directional_light_shadow_slice` to be
launched and observed interactively. A successful build alone is not enough.

## Coordinate Convention

GFX-M4-0 freezes the current graphics coordinate convention before native
SagaGraphics resource migration begins. The contract is recorded in
[GRAPHICS_COORDINATE_CONVENTION.md](GRAPHICS_COORDINATE_CONVENTION.md).

The automated coordinate claim is:

```text
GRAPHICS COORDINATE CONVENTION VERIFIED
```

This claim is limited to the current CPU math contract and the Diligent/Vulkan
runtime fixtures listed below. It does not imply RenderGraph GPU execution or
native SagaGraphics resource ownership.

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
| `DiligentGPU.DirectionalLightChangesSurfaceLuminance` | Directional light direction changes pixel luminance. |
| `DiligentGPU.VertexNormalsAffectLighting` | Vertex normals affect the GPU lighting result. |
| `DiligentGPU.AmbientLightKeepsUnlitFacingSurfaceVisible` | Ambient contribution remains visible when diffuse is unavailable. |
| `DiligentGPU.UnlitMaterialIgnoresDirectionalLight` | The M2 unlit path remains compatible. |
| `DiligentGPU.ShadowPassSubmitsDepthDraws` | Shadow-enabled views execute a depth-only shadow pass. |
| `DiligentGPU.ShadowMapResourceIsPersistentAcrossFrames` | Shadow map resources are reused across frames. |
| `DiligentGPU.ShadowEnabledFrameRejectsAdditionalSubmit` | Shadow-enabled frames enforce the single-submit contract. |
| `DiligentGPU.ShadowSamplingProducesValidatedPixels` | Lit draws bind and sample the shadow map path with pixel output present. |
| `DiligentGPU.OccluderCastsShadowOnReceiver` | A caster projects a shadow onto a receiver. |
| `DiligentGPU.DisablingShadowsRestoresReceiverLuminance` | Shadow toggle removes receiver darkening. |
| `DiligentGPU.ShadowResultIsIndependentOfMainPassDrawOrder` | Projected shadow result remains stable across main-pass draw order changes. |
| `DiligentGPU.MovingOccluderMovesProjectedShadow` | Updated model transforms move the projected shadow. |
| `DiligentGPU.ShadowMapOutsideProjectionRemainsLit` | Pixels outside shadow projection are treated as lit. |
| `DiligentGPU.ShadowPcfConfigPropagatesConfiguredResolution` | Shadow resolution diagnostics/config propagation reach the shadow sampling path. |
| `DiligentGPU.LightingAndShadowsRemainValidAcrossMultipleFrames` | Shadow resources and diagnostics remain stable across frames. |
| `DiligentGPU.ResizePreservesLightingAndShadows` | Swapchain resize preserves the persistent shadow map path. |
| `DiligentGPU.ShadowToggleDoesNotLeakOrUseStaleBindings` | Disabling and re-enabling shadows does not use stale visible output. |
| `GraphicsCoordinateConvention.PerspectiveNearMapsToExpectedDepth` | PerspectiveRH_ZO maps the near plane to canonical depth 0. |
| `GraphicsCoordinateConvention.PerspectiveFarMapsToExpectedDepth` | PerspectiveRH_ZO maps the far plane to canonical depth 1. |
| `GraphicsCoordinateConvention.OrthographicNearFarMapToZeroOne` | OrthoRH_ZO uses the same zero-to-one depth convention. |
| `GraphicsCoordinateConvention.ViewForwardIsNegativeZ` | Canonical camera forward is `-Z`. |
| `GraphicsCoordinateConvention.ProjectionViewModelOrderIsPVM` | CPU math contract is `projection * view * model * position`. |
| `GraphicsCoordinateConvention.CounterClockwiseTriangleIsFrontFacing` | CCW winding is the canonical front-face convention. |
| `GraphicsCoordinateConvention.CanonicalUvCornersMapCorrectly` | Canonical texture UV corners use top-left texture semantics. |
| `GraphicsCoordinateConvention.ShadowNdcToUvMapsCenterToHalfHalf` | Shadow NDC-to-UV maps center and vertical orientation deterministically. |
| `GraphicsCoordinateConvention.NormalTransformUsesInverseTranspose` | Normal transforms use inverse-transpose semantics. |
| `GraphicsCoordinateConvention.BackendAdjustmentAppliedExactlyOnce` | Shadow NDC-to-UV owns the covered Y inversion, avoiding double application. |
| `CoordinateGPU.CounterClockwiseTriangleDraws` | CCW geometry survives back-face culling and produces pixels. |
| `CoordinateGPU.ClockwiseTriangleIsCulled` | CW geometry is culled with the default back-face cull policy. |
| `CoordinateGPU.NearGeometryOccludesFarGeometry` | Diligent/Vulkan depth output follows the canonical near/far convention. |
| `CoordinateGPU.TextureCornersMatchCanonicalUv` | Uploaded texture quadrants match canonical UV corner semantics. |
| `CoordinateGPU.ShadowProjectionMatchesMainConvention` | Shadow projection and main camera convention agree in a pixel shadow fixture. |
| `CoordinateGPU.BackendAdjustmentIsNotDoubleApplied` | Vertical positioning is not double-flipped by backend adjustment. |

## Sandbox Slice

The runtime scenario id is:

```text
first_3d_vertical_slice
directional_light_shadow_slice
```

Manual launch command:

```bash
nix-shell --run "<build-dir>/bin/SagaSandbox first_3d_vertical_slice --vulkan"
nix-shell --run "<build-dir>/bin/SagaSandbox directional_light_shadow_slice --vulkan"
```

Use the active configured build directory, for example the current
`build/RelWithDebInfo-*` directory. Durable architecture docs should not pin
changing `0.0.x` build folder names. See
[NIX_DEVELOPMENT_AND_VALIDATION_POLICY.md](NIX_DEVELOPMENT_AND_VALIDATION_POLICY.md).

The scenario owns its resources in `OnInit()` and destroys them in
`OnShutdown()`. The M2 scenario creates a ground plane, multiple textured cubes,
perspective camera output, depth-tested opaque materials, and a rotating cube.
The M3 scenario uses lit diffuse materials, one directional light, ambient
light, shadow-map sampling, a moving occluder, shadow toggle, and light
rotation. Movement uses WASD with mouse look through the existing Sandbox input
stack.

## Explicit Non-Claims

This slice does not complete R4, R4B, or R5.

It does not prove:

- native SagaGraphics resource upload;
- native descriptor set/ring-buffer/frame-resource integration;
- RenderGraph-to-Diligent execution;
- asset-driven scene loading;
- material graph authoring;
- PBR, cascaded shadows, point/spot lights, HDR/tonemapping, post-processing,
  terrain, water, VFX, or character rendering completion;
- production renderer readiness.

The automated M3 validation runs did not enable Vulkan validation layers. The
current D32 shadow depth texture with SRV sampling is verified on the tested
NVIDIA/Vulkan configuration only, not as a full backend portability guarantee.
