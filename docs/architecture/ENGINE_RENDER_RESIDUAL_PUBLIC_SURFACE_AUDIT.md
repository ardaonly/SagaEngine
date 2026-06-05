# Engine Render Residual Public Surface Audit

> Last updated: 2026-06-05

This report-only audit covers the remaining public Render include surface after
the `RenderPipelineConfig` shell, the post/shadow config DTO shell, and the
low-risk private moves for graph compilation, built-in pass stubs, frame
execution, command recording, and the old renderer shell.

This document does not move headers, change CMake/install rules, or claim a
stable render SDK/API. The Render public/private boundary is still not fixed.

## Current Public Surface

Remaining public Render headers:

- `SagaEngine/Render/RenderPipelineConfig.h`
- `SagaEngine/Render/RenderCore.h`
- `SagaEngine/Render/RenderGraph/RenderGraph.h`
- `SagaEngine/Render/RenderGraph/RGPass.h`
- `SagaEngine/Render/RenderGraph/RGTypes.h`
- `SagaEngine/Render/RenderPasses/PostProcessGraph.h`
- `SagaEngine/Render/RenderPasses/ShadowMap.h`
- `SagaEngine/Render/Backend/IRenderBackend.h`
- `SagaEngine/Render/Backend/RenderBackendFactory.h`
- `SagaEngine/Render/RenderSubsystem.h`
- `SagaEngine/Render/World/RenderWorld.h`
- `SagaEngine/Render/World/RenderEntity.h`
- `SagaEngine/Render/Culling/CullingPipeline.h`
- `SagaEngine/Render/LOD/LODSelector.h`
- `SagaEngine/Render/Materials/Material.h`
- `SagaEngine/Render/Materials/MeshAsset.h`
- `SagaEngine/Render/Scene/Camera.h`
- `SagaEngine/Render/Scene/FrameContext.h`
- `SagaEngine/Render/Scene/Frustum.h`
- `SagaEngine/Render/Scene/RenderView.h`
- `SagaEngine/Render/Bridge/NetEntityBinding.h`

The completed safe moves removed these implementation headers from the public
surface without forwarding headers:

- `RGCompilation.h`
- `GBufferPass.h`
- `LightingPass.h`
- `FrameGraphExecutor.h`
- `CommandBuffer.h`
- `CommandRecorder.h`
- `Renderer.h`

## Classification

| Header group | Classification | Next action |
| --- | --- | --- |
| `RenderPipelineConfig.h` | Config-first public shell | Keep public and evolve only with scalar/config/status/stat DTOs. Post/shadow profile DTO coverage now exists here. |
| `RenderCore.h` | Transitional shared values | Keep public for now; reassess whether `RenderConfig` should be bridged to `RenderPipelineConfig`. |
| `RenderGraph.h`, `RGPass.h`, `RGTypes.h` | Public-internal graph debt | Replace public graph/pass signatures with config/facade-owned DTOs before moving private. |
| `PostProcessGraph.h`, `ShadowMap.h` | Declarative config debt | Map concepts into `RenderPipelineConfig`-adjacent DTOs before moving private. |
| `IRenderBackend.h`, `RenderBackendFactory.h` | Backend/runtime contract candidate | Audit separately; may be legitimate public backend integration surface. |
| `RenderSubsystem.h`, `RenderWorld.h`, `RenderEntity.h`, `CullingPipeline.h` | Runtime composition review | Audit separately before moving; these may be client/runtime composition contracts. |
| Materials, mesh, scene, LOD, bridge headers | Shared render data candidates | Keep public until a focused dependency audit proves otherwise. |

## Residual Debt

`RenderGraph.h` and `RGPass.h` still expose graph construction, pass dependency
lists, execution order, and `RHI::IRHI&` callbacks. `RGTypes.h` still exposes
render graph resource descriptors backed by public `RHI/Types`. Those are not
appropriate as the stable public render extension boundary.

`PostProcessGraph.h` and `ShadowMap.h` are closer to declarative config than
execution API, but they currently sit under `RenderPasses` and describe
renderer/pass concepts directly. Public scalar DTO coverage for the compact
post-processing profile and shadow profile now lives in `RenderPipelineConfig.h`.
The old headers remain public-internal debt until a later private move removes
or remaps their remaining direct surface.

Backend and runtime composition headers are intentionally not moved in this
phase. They need a separate decision because backend interfaces, render world
data, culling, scene view data, and material/mesh DTOs may be valid public
contracts for tools, clients, or backend integration.

## Recommended Sequence

1. Keep `RenderPipelineConfig.h` as the only new public config-first render
   boundary. Do not introduce custom pass registration yet.
2. Keep the new post-processing and shadow profile DTOs scalar/config-owned;
   do not expose graph, pass, RHI, material vector, or atlas descriptor types
   through the public config shell.
3. Stop public use of graph/pass execution signatures before moving
   `RenderGraph.h`, `RGPass.h`, and `RGTypes.h` private.
4. Move `PostProcessGraph.h` and `ShadowMap.h` private only after their public
   configuration role is fully mapped to the config DTO shell.
5. Run a separate backend/runtime audit for `IRenderBackend.h`,
   `RenderBackendFactory.h`, `RenderSubsystem.h`, `RenderWorld.h`,
   `CullingPipeline.h`, material/mesh, scene, LOD, and bridge headers.

## Non-Goals

- no header moves in this audit;
- no source, test, CMake, or install rule changes;
- no RHI/Types migration;
- no World changes;
- no stable render SDK/API claim;
- no claim that the Render public/private boundary is fixed.
