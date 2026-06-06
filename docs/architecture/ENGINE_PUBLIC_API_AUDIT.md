# Engine Public API Audit

> Last updated: 2026-06-06

`Engine/Public/SagaEngine` is the installed development include surface for
`SagaEngine`. That makes every header in this tree part of the visible engine
API, even when the type is mostly used by engine internals.

This audit records the current shape before moving headers. The first cleanup
step is classification, not relocation.

## Current Snapshot

- Public headers under `Engine/Public/SagaEngine`: 266.
- Private implementation files under `Engine/Private/SagaEngine`: 175.
- Public header lines: 28,283.
- Private implementation lines: 36,883.
- Largest public areas by header count: `Core`, `Diagnostics`, `Render`,
  `Client`, `Input`, `Scripting`, and `Resources`.

The counts above were not recomputed during the WorldFacade shell,
WorldNode/SimCell cluster private migration, RenderPipelineConfig shell, or
low-risk Render header private migration phases. Those phases added
`SagaEngine/World/WorldFacade.h` as a narrow transitional public facade shell,
moved the old WorldNode/SimCell cluster private, added and extended
`SagaEngine/Render/RenderPipelineConfig.h` as a narrow transitional public
render config shell with post/shadow DTO coverage, and moved `RGCompilation.h`,
`GBufferPass.h`,
`LightingPass.h`, `FrameGraphExecutor.h`, `CommandBuffer.h`,
`CommandRecorder.h`, and `Renderer.h` private, then added the first
`SagaEngine/Graphics` vendor-neutral public shell.

The follow-up World closure phase audited `cmake/modules/SagaInstall.cmake`.
The install rule still installs `Engine/Public/SagaEngine` wholesale via
`install(DIRECTORY "${SAGA_ROOT}/Engine/Public/SagaEngine" ...)`; with the
current tree, the installed World include surface would include only
`SagaEngine/World/WorldFacade.h`. The rule was not changed in that phase.

Many public headers are not included outside `Engine` today. That does not make
them safe to delete, but it does mean the installed API surface is broader than
the current external use pattern.

## Public API Criteria

A header should stay public when it is one of these:

- a stable type that app, runtime, server, editor, tool, or tests need to
  include directly;
- an interface or contract intended for backend, platform, resource, scripting,
  UI, or package integration;
- a small data type that is naturally shared across engine boundaries;
- an installed contract that would be unreasonable to access through a facade.

A header should be reviewed for private/internal placement when it is one of
these:

- a manager, builder, executor, pipeline, tracker, or cache that coordinates
  implementation lifecycle;
- a render pass, render graph, world partition, replication, or diagnostics
  implementation detail;
- a concrete backend, platform, importer, or server policy type;
- a role-specific type that belongs under `Runtime`, `Server`, `Editor`, or a
  tool library instead of the core engine include surface.

## Classification

| Area | Current classification | Notes |
| --- | --- | --- |
| `SagaEngine/Math` | Keep public | Shared math primitives and deterministic helpers. |
| `SagaEngine/ECS` | Keep public, review large helpers | Core entity/component types are shared; executor/observer helpers may need a later pass. |
| `SagaEngine/Simulation` | Keep public | Shared deterministic simulation contracts. |
| `SagaEngine/UI/I*` | Keep public | Backend-facing interfaces and IDs. |
| `SagaEngine/Platform/I*` | Keep public | Platform abstraction interfaces. |
| `SagaEngine/Packages` and `SagaEngine/Assets` manifests | Keep public | Package and asset manifest contracts are cross-module inputs. |
| `SagaEngine/World` | Transitional facade only | The old WorldNode/SimCell cluster and Partition/Streaming headers are private. `WorldFacade.h` is the public shell, not a stable SDK guarantee. See [Engine World public header audit](ENGINE_WORLD_PUBLIC_HEADER_AUDIT.md), the focused [Engine WorldNode / SimCell facade audit](ENGINE_WORLD_NODE_FACADE_AUDIT.md), and [Engine World public facade contract](ENGINE_WORLD_PUBLIC_FACADE_CONTRACT.md). |
| `SagaEngine/Graphics` | Foundation shell | Vendor-neutral graphics vocabulary and null backend shell for the Diligent migration path. The R2B-lite guard covers public/install surface leakage for this new shell; it is not a stable external SDK claim and does not move `SagaEngine/Render/Backend`. See [Render Public API Contract](RENDER_PUBLIC_API_CONTRACT.md). |
| `SagaEngine/Render/RenderPipelineConfig.h` | Transitional config shell | Scalar/config-owned DTO surface for public render profile data, including compact post-processing and shadow profile settings. It is not a stable render SDK/API claim and does not fix the Render public/private boundary. See [Engine Render Extension / Config boundary contract](ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md). |
| `SagaEngine/Render/RenderGraph` | Internal candidate | Render graph construction remains public-internal until a stable public render extension model exists. `RGCompilation.h` and frame execution internals have been moved private. See [Engine RenderGraph / RenderPasses public header audit](ENGINE_RENDERGRAPH_PUBLIC_HEADER_AUDIT.md), [Engine Render Extension / Config boundary contract](ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md), and [Engine Render residual public surface audit](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md). |
| `SagaEngine/Render/RenderPasses` | Internal candidate | Built-in pass classes `GBufferPass` and `LightingPass` have been moved private; post/shadow config DTO coverage now exists in `RenderPipelineConfig.h`, but `PostProcessGraph.h` and `ShadowMap.h` remain internal candidates until the actual private move. See [Engine RenderGraph / RenderPasses public header audit](ENGINE_RENDERGRAPH_PUBLIC_HEADER_AUDIT.md), [Engine Render Extension / Config boundary contract](ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md), and [Engine Render residual public surface audit](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md). |
| `SagaEngine/RHI/Types` | Internal candidate | Backend-facing resource descriptors may need a narrower facade before install exposure. |
| `SagaEngine/Diagnostics/*Record`, `*Snapshot`, `*Tracker`, `*Builder` | Internal candidate | Useful for tests and tools, but much of it is reporting infrastructure rather than engine API. |
| `SagaEngine/Client/Replication` | Review later | Some wire/state contracts may be shared; recovery, telemetry, cache, and scheduler types look internal. |
| `SagaEngine/Scripting/LowLevel` and `SagaEngine/Scripting/Authoring` | Review later | The split is useful, but installed surface should separate stable host contracts from tool-authoring details. |
| `SagaEngine/Resources/*Manager`, `*Pipeline`, `*Streamer`, importers | Review later | Public contracts should likely be asset IDs, sources, registries, and request/status types. |

## First Move Candidates

The first completed World move covered only the Partition/Streaming cluster
listed in [Engine World public header audit](ENGINE_WORLD_PUBLIC_HEADER_AUDIT.md).
For future cleanup passes, continue with headers that are not included outside
`Engine` and are already implementation oriented:

- remaining `SagaEngine/World` facade behavior work, keeping the private
  cluster behind the boundary defined in
  [Engine World public facade contract](ENGINE_WORLD_PUBLIC_FACADE_CONTRACT.md)
- `SagaEngine/Render/RenderGraph`
- `SagaEngine/Render/RenderPasses`
- `SagaEngine/RHI/Types`

Recommended next non-World cleanup candidate remains the residual
`SagaEngine/Render/RenderGraph` plus `SagaEngine/Render/RenderPasses` public
surface after the low-risk `RGCompilation.h`, `GBufferPass.h`, and
`LightingPass.h` private move and the frame execution/command recording private
move. `SagaEngine/RHI/Types` should wait because public RHI headers still
include those descriptors directly; Diagnostics should wait because server,
tools, and tests actively consume its reporting surface.

The focused [Engine RenderGraph / RenderPasses public header audit](ENGINE_RENDERGRAPH_PUBLIC_HEADER_AUDIT.md)
recommends designing a narrow render extension/config boundary before moving
the full cluster. The follow-up
[Engine Render Extension / Config boundary contract](ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md)
recommended `RenderPipelineConfig` as the conservative public config-first
surface. `SagaEngine/Render/RenderPipelineConfig.h` now provides the first
header-only shell for that surface, including compact post/shadow config DTOs,
while RenderGraph and RenderPasses remain internal candidates. The low-risk
implementation headers `RGCompilation.h`, `GBufferPass.h`, `LightingPass.h`,
`FrameGraphExecutor.h`, `CommandBuffer.h`, `CommandRecorder.h`, and
`Renderer.h` have been removed from the public surface without adding
forwarding headers.

The focused
[Engine Render residual public surface audit](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md)
is the current starting point for any further Render public/private cleanup.

Each move should be small, update include paths in `Engine/Private`, and run the
architecture test plus the affected subsystem tests.

## Guardrail

`PublicPrivateBoundaryTests` keeps concrete backend/server paths out of public
headers and checks that internal-candidate public buckets remain documented
here. This is intentional technical debt tracking, not approval to grow the
public surface.
