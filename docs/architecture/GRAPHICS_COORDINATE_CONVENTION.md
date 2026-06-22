# Graphics Coordinate Convention

> Last updated: 2026-06-23

This document records the canonical SagaEngine graphics coordinate contract
used by the current Diligent/Vulkan renderer and by the upcoming SagaGraphics
resource migration.

## Canonical Space

SagaEngine uses a right-handed world:

- `+X` points right.
- `+Y` points up.
- `-Z` is camera forward.
- Camera view matrices are built with `Mat4::LookAtRH`.
- Main camera projections use `Mat4::PerspectiveRH_ZO`.
- Directional shadow projections use `Mat4::OrthoRH_ZO`.
- Canonical clip-space depth is `[0, 1]`.

Matrices use column-major storage and column-vector mathematical convention.
Composition order is:

```text
clip = projection * view * model * position
```

`Camera::RecomputeViewProj()` therefore computes:

```text
viewProj = projection * view
```

Model composition follows the same convention. `parent * child` transforms a
child-space vector into parent/world space, and `Mat4::FromTransform()` builds
`T * R * S`.

## Rasterization

The canonical front face is counter-clockwise. The default opaque material
culls back faces. Diligent PSO setup sets `FrontCounterClockwise = True` and
maps Saga material cull modes to Diligent rasterizer cull modes.

This means a triangle whose vertices are counter-clockwise in projected screen
space is front-facing. Clockwise triangles are back-facing when back-face cull
is enabled.

## Texture and Image Coordinates

Canonical mesh UVs use a top-left texture semantic:

- `u = 0, v = 0` samples the top-left texel.
- `u = 1, v = 0` samples the top-right texel.
- `u = 0, v = 1` samples the bottom-left texel.
- CPU texture payloads are supplied in row-major order from top row to bottom
  row.

This is a material/asset convention. It is distinct from framebuffer origin and
from backend clip-space adjustment.

Normal-map tangent-space Y is not certified by the current runtime tests. Until
the asset pipeline chooses and tests a normal-map convention, normal maps remain
outside the verified claim set.

## Shadow UV Mapping

The current shadow shader maps canonical light NDC into shadow texture UVs as:

```text
uv.x = ndc.x * 0.5 + 0.5
uv.y = 0.5 - ndc.y * 0.5
```

This is classified as the canonical shadow NDC-to-texture transform, not as a
Vulkan backend workaround. Backend clip adjustment must not apply a second
shadow Y flip.

Coordinates outside the shadow map projection are treated as lit by the current
LitDiffuse shadow path.

## Backend Boundary

Public camera, scene, material, and RenderGraph code must not contain
backend-specific Vulkan/D3D/Metal clip fixes. Backend-specific adjustments, if
needed in future work, belong in private graphics adapter code and must be
tested so they are applied exactly once.

Current Vulkan tests verify that the present renderer does not double-apply the
vertical projection/texture adjustment for the covered opaque and shadow cases.

## Depth Policy

The current verified policy is conventional non-reversed depth:

- near plane maps to depth `0`;
- far plane maps to depth `1`;
- nearer geometry occludes farther geometry with the existing depth state.

Reverse-Z and infinite far-plane projections are future work and are not part
of the current verified convention.

## Claim Boundary

The coordinate convention is verified when the CPU and GPU coordinate tests pass
with the Diligent/Vulkan runtime path. This does not claim:

- RenderGraph GPU execution;
- SagaGraphics native resource ownership;
- normal-map pipeline correctness;
- cross-backend Windows parity;
- reverse-Z;
- device-loss recovery.
