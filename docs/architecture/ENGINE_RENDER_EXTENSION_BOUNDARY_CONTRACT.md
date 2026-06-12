# Engine Render Extension / Config Boundary Contract

> Last updated: 2026-06-06

This document records a conservative public render boundary direction before
any RenderGraph or RenderPasses header migration. It follows the recommendation
from
[Engine RenderGraph / RenderPasses public header audit](ENGINE_RENDERGRAPH_PUBLIC_HEADER_AUDIT.md):
design a narrow render extension/config boundary before moving the current
public-internal RenderGraph and RenderPasses cluster.
The follow-up
[Engine Render residual public surface audit](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md)
classifies the remaining public Render surface after the safe private moves.

This is a design record, not a stable render SDK or extension API guarantee.
The Render public/private boundary is not fixed by this document.

## Current Context

`Engine/Public/SagaEngine/Render/RenderGraph` and
`Engine/Public/SagaEngine/Render/RenderPasses` are currently visible through
the installed engine include surface. The focused audit did not find direct
includes from `Tests`, `Tools`, `Runtime`, `Server`, `Editor`, or `Apps`, but
the headers still expose implementation-shaped renderer concepts:

- graph resource IDs and descriptors;
- pass scheduling and resource usage;
- graph compilation and schedule output;
- execution callbacks over `RHI::IRHI&`;
- built-in deferred pass implementation classes;
- declarative post-process and shadow data without a settled public config
  boundary.

That shape is too broad to treat as a stable render extension API. The safer
boundary is config-first: public callers describe desired render profiles and
features with copied data, while private renderer code owns graph scheduling,
pass implementation, backend command recording, and transient resource
lifetime.

The RenderPipelineConfig shell milestone adds
`SagaEngine/Render/RenderPipelineConfig.h` as the first public config-owned DTO
surface. It is header-only and contains scalar/config/status/stat data only.
It does not replace existing `RenderConfig`, wire renderer behavior, or move
the current RenderGraph or RenderPasses headers.

The post/shadow DTO shell extends that same header with scalar-only
post-processing profile and shadow profile configuration. That gives the public
boundary compact settings for post effects, output format, tone mapping,
exposure, color grading, atlas size, caster limits, cascades, distance, and
bias values without exposing post-process graph data, shadow pass descriptors,
material vectors, graph types, or backend/RHI handles.

The first low-risk render header migration after the shell moved
`RGCompilation.h`, `GBufferPass.h`, and `LightingPass.h` private. That slice
did not move `RenderGraph.h`, `RGPass.h`, `RGTypes.h`, `PostProcessGraph.h`,
or `ShadowMap.h`, and it did not add public forwarding headers.

The next safe render move also moved `FrameGraphExecutor.h`,
`CommandBuffer.h`, `CommandRecorder.h`, and `Renderer.h` private. That keeps
frame execution, command recording, and the old renderer shell out of the
public include surface while leaving the config-first boundary intact.

The `SagaEngine/Graphics` public shell is the next graphics-boundary step for
the Diligent migration path. Its R2B-lite guard protects the new
vendor-neutral public/install surface from Diligent or native graphics API
leaks. It does not move `SagaEngine/Render/Backend`, does not add
RenderGraph/material/shader/resource behavior, and is not a stable external SDK
claim.

The current public/install graphics surface rules are recorded in
[Render Public API Contract](RENDER_PUBLIC_API_CONTRACT.md).

## Future Public Boundary

Allowed future public render responsibilities:

- high-level render feature configuration;
- declarative render pipeline/profile configuration;
- stable render option, status, and stat DTOs;
- editor/tooling-safe configuration descriptions;
- controlled custom pass registration only after a future extension model is
  explicitly designed.

Forbidden future public render responsibilities:

- concrete graph scheduler internals;
- frame graph compilation internals;
- raw IRHI execution callbacks;
- backend command recording internals;
- built-in pass implementation classes;
- transient resource lifetime internals;
- private renderer ownership/lifecycle internals.

Public render configuration may use copied, scalar, and config-oriented DTOs.
Allowed public signature inputs and outputs include:

- render pipeline/profile config structs;
- feature toggle structs;
- option, status, stat, and capability structs;
- stable scalar identifiers and counts;
- stable public math or resource identifiers where a config value needs them;
- copied editor/tooling descriptions that do not expose mutable renderer state.

Public render signatures should avoid exposing:

- `RHI::IRHI` in user execution callbacks;
- command buffers, command recorders, or backend command-list objects;
- graph compiler, graph scheduler, compiled graph, or frame graph executor
  types;
- concrete built-in pass classes;
- mutable transient resource handles or lifetime internals;
- concrete renderer ownership, lifetime, or implementation objects.

## Naming Decision

| Candidate | Assessment |
| --- | --- |
| `RenderPipelineConfig` | Recommended config-first public name for declarative pipeline/profile settings. It keeps the boundary on data the renderer can validate and translate privately. |
| `RenderFeatureSet` | Useful supporting DTO name for feature toggles, but too narrow to be the whole boundary because profiles need more than on/off features. |
| `RenderExtensionRegistry` | Defer until plugin/custom pass semantics exist. Introducing a registry name now would imply extension lifecycle and compatibility rules that are not designed. |
| `RenderGraphFacade` | Reject for now because it centers graph internals and risks making private scheduling concepts the public contract. |
| `RenderPipelineFacade` | Defer until there is real behavior behind a facade. A facade name should wait for an implementation slice that owns lifecycle, validation, or execution. |

Recommended conservative boundary: introduce `RenderPipelineConfig` as the
primary public config name, with supporting `RenderFeatureSet`, status, stat,
option, and capability DTOs later as needed.

Do not introduce custom pass registration yet. Controlled registration should
wait for an explicit render extension model that defines lifecycle, ordering,
resource declarations, validation, backend access limits, compatibility, and
failure behavior.

## Header Disposition

Future disposition of the audited headers:

| Header | Future disposition |
| --- | --- |
| `SagaEngine/Render/RenderGraph/RGCompilation.h` | Moved private in the first low-risk migration slice after the config shell. It exposes graph compilation and schedule output internals. |
| `SagaEngine/Render/RenderGraph/RGPass.h` | Move private once public signatures stop exposing pass callbacks and resource usage declarations. |
| `SagaEngine/Render/RenderGraph/RenderGraph.h` | Keep public-internal until replaced by a config or facade boundary, then move private. |
| `SagaEngine/Render/RenderGraph/RGTypes.h` | Keep public-internal until RenderPasses no longer depend on it publicly. |
| `SagaEngine/Render/RenderPasses/GBufferPass.h` | Moved private as a built-in pass implementation detail. |
| `SagaEngine/Render/RenderPasses/LightingPass.h` | Moved private as a built-in pass implementation detail. |
| `SagaEngine/Render/RenderPasses/PostProcessGraph.h` | Keep public-internal until the existing direct surface is removed or fully mapped to the post config DTO shell. |
| `SagaEngine/Render/RenderPasses/ShadowMap.h` | Keep public-internal until the existing direct surface is removed or fully mapped to the shadow config DTO shell. |
| `SagaEngine/Render/FrameGraphExecutor.h` | Moved private as frame execution internals. |
| `SagaEngine/Render/CommandRecording/CommandBuffer.h` | Moved private as command recording internals. |
| `SagaEngine/Render/CommandRecording/CommandRecorder.h` | Moved private as command recording internals. |
| `SagaEngine/Render/Renderer.h` | Moved private as the old renderer implementation shell. |

## Transition Strategy

1. Keep the current RenderGraph/RenderPasses cluster classified as
   public-internal debt while the narrow public render config shell matures.
2. Keep the first public render shell as data-only configuration, not a graph
   or pass execution API.
3. Use `RenderPipelineConfig` for declarative pipeline/profile settings and
   `RenderFeatureSet` only where feature toggles need a named DTO.
4. Keep graph scheduling, graph compilation, `IRHI` execution, command
   recording, built-in pass classes, transient resource lifetime, and private
   renderer ownership behind private implementation boundaries.
5. Keep `RGCompilation.h`, `GBufferPass.h`, and `LightingPass.h` private; do
   not add forwarding public compatibility headers.
6. Keep frame execution, command recording, and the old renderer shell private;
   do not add forwarding public compatibility headers.
7. Keep the post/shadow config DTO shell as the public config-first step for
   those concepts; do not move `PostProcessGraph.h` or `ShadowMap.h` until the
   remaining direct use and mapping details are handled.
8. Reassess `RenderGraph.h`, `RGPass.h`, `RGTypes.h`, `PostProcessGraph.h`,
   and `ShadowMap.h` after the config shell proves what data must remain
   public. See the focused
   [Engine Render residual public surface audit](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md).

## Non-Goals

- no stable render SDK claim;
- no custom pass/plugin API claim;
- no remaining RenderGraph or RenderPasses migration claim;
- no CMake or install rule changes;
- no backend command recording exposure;
- no World boundary changes.

## Next Implementation Slice

The next implementation slice should keep post-processing, shadow, and existing
render configuration flowing through `RenderPipelineConfig` before any private
header migration. The repository already has `RenderConfig`, but it does not
yet have a mature extension registry or custom pass API. Keep that work
config-first so editor/tooling-safe render profile data stays separate from
private RenderGraph scheduling, `IRHI` callbacks, command recording, and
built-in pass classes before moving the remaining public-internal cluster. The
focused
[Engine Render residual public surface audit](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md)
records the recommended order.
