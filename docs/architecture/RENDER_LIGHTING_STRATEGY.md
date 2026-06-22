# Render Lighting Strategy

> Last updated: 2026-06-22

Current path:

```text
IRenderBackend -> DiligentRenderBackend -> Vulkan
```

This is the short-term canonical runtime renderer. It is separate from the
future SagaGraphics / R4 / R4B / R5 migration foundation.

## Current Model

The current forward opaque lighting path supports:

- one directional light;
- ambient light;
- `OpaqueShadingModel::Unlit`;
- `OpaqueShadingModel::LitDiffuse`;
- vertex-normal Lambert diffuse lighting;
- one persistent orthographic directional shadow map;
- manual depth comparison in the main pass;
- 3x3 PCF using texel size derived from the active shadow resolution.

Directional light direction is the world-space direction light rays travel.
Shaders use `-direction` as the vector from the surface toward the light.

The minimum lighting equation is:

```text
ambient = albedo * ambientColor * ambientIntensity
diffuse = albedo * lightColor * lightIntensity * max(dot(normal, -lightDirection), 0)
final   = ambient + diffuse
```

## Shadow Strategy

Current shadow strategy:

```text
single directional-light orthographic shadow map
```

The current private shadow settings are:

- shadow resolution: 1024 x 1024;
- orthographic light volume;
- constant bias;
- normal/slope-aware bias;
- PCF radius 1, producing 3x3 filtering.

Shadow resources are private to `DiligentRenderBackend`: depth texture, DSV,
SRV, shadow PSO, shadow shader, and constant data. They are not exposed through
`IRenderBackend`, SagaGraphics, or installed public graphics headers.

The current shadow map uses `TEX_FORMAT_D32_FLOAT` with DSV and SRV views. That
path is verified on the tested NVIDIA/Vulkan configuration only; cross-vendor
or cross-backend depth-SRV fallback selection is still future work.

Shadow-enabled frames accept one `RenderView` submit. Additional shadow-enabled
submits in the same frame are rejected and reported by concrete backend
diagnostics. Shadows-disabled legacy submits keep the previous behavior.

## Color Space

M3 uses policy B from the milestone contract: procedural/test albedo textures
are treated as linear-valued `RGBA8_UNORM` inputs. This milestone does not claim
asset-driven sRGB albedo decode or a color-managed material pipeline.

The swapchain may be SRGB. That is distinct from albedo texture SRGB decode.
The lighting shader does not apply a second manual gamma curve.

## Claim Mapping

```text
DIRECTIONAL LIGHTING PIXEL-VERIFIED
```

Evidence:

- `DiligentGPU.DirectionalLightChangesSurfaceLuminance`
- `DiligentGPU.VertexNormalsAffectLighting`
- `DiligentGPU.AmbientLightKeepsUnlitFacingSurfaceVisible`
- `DiligentGPU.UnlitMaterialIgnoresDirectionalLight`

```text
SHADOW OCCLUSION PIXEL-VERIFIED
```

Evidence:

- `DiligentGPU.OccluderCastsShadowOnReceiver`
- `DiligentGPU.DisablingShadowsRestoresReceiverLuminance`
- `DiligentGPU.ShadowResultIsIndependentOfMainPassDrawOrder`
- `DiligentGPU.MovingOccluderMovesProjectedShadow`
- `DiligentGPU.ShadowMapOutsideProjectionRemainsLit`
- `DiligentGPU.ShadowPcfConfigPropagatesConfiguredResolution`
- `DiligentGPU.LightingAndShadowsRemainValidAcrossMultipleFrames`
- `DiligentGPU.ResizePreservesLightingAndShadows`
- `DiligentGPU.ShadowToggleDoesNotLeakOrUseStaleBindings`

```text
BASIC LIT PLAYABLE 3D SLICE VERIFIED
```

Requires manual observation of:

```bash
nix-shell --run "<build-dir>/bin/SagaSandbox directional_light_shadow_slice --vulkan"
```

A successful build alone does not establish this claim.

## Explicit Non-Claims

The current lighting path does not claim:

- PBR;
- normal mapping;
- cascaded shadow maps;
- point lights;
- spot lights;
- HDR;
- tonemapping;
- transparent-object shadows;
- alpha-tested shadow casters;
- skinned mesh shadows;
- terrain/HLOD shadows;
- RenderGraph execution;
- SagaGraphics migration;
- device-loss recreation;
- production quality or performance certification;
- asset-driven sRGB albedo color management;
- Vulkan validation-layer clean execution. Current automated M3 runs do not
  enable Vulkan validation layers.
