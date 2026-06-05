# Engine RenderGraph / RenderPasses Public Header Audit

> Last updated: 2026-06-05

This report-only audit covers the public headers under:

- `Engine/Public/SagaEngine/Render/RenderGraph`
- `Engine/Public/SagaEngine/Render/RenderPasses`

This audit began as a report-only review. Later recovery slices added the
`RenderPipelineConfig` public shell and moved the low-risk
`RGCompilation.h`, `GBufferPass.h`, and `LightingPass.h` headers private. A
following safe-move slice moved `FrameGraphExecutor.h`, `CommandBuffer.h`,
`CommandRecorder.h`, and `Renderer.h` private. No CMake/install rules or
non-render subsystems were changed by those render slices.

The Render public/private boundary is not fixed by this document. The purpose
is to identify the safest later migration or facade-design slice.

## Method

Direct includes are the primary signal. Broad symbol searches are supporting
context only because names such as `RenderPass`, `RenderGraph`, and
`FrameGraph` appear in comments, renderer internals, and unrelated tooling
examples.

Read-only inspection commands:

```sh
find Engine/Public/SagaEngine/Render/RenderGraph -type f
find Engine/Public/SagaEngine/Render/RenderPasses -type f
rg -n '#include [<"]SagaEngine/Render/RenderGraph/' . -g '!build/**' -g '!Build/**'
rg -n '#include [<"]SagaEngine/Render/RenderPasses/' . -g '!build/**' -g '!Build/**'
rg -n 'RenderGraph|RenderPass|FrameGraph|RenderPasses|CommandRecording|FrameGraphExecutor' Engine Tests Tools Runtime Server Editor Apps -g '!build/**' -g '!Build/**'
```

Read-only findings:

- No direct includes of these headers were found from `Tests`, `Tools`,
  `Runtime`, `Server`, `Editor`, or `Apps`.
- Direct include use is limited to public render headers including each other
  and matching implementation files under `Engine/Private/SagaEngine/Render`.
- `Tools/Prism` search hits are documentation/example text, not includes of the
  SagaEngine render headers.

## Direct Include Matrix

| Header | Outside Engine direct includes | Engine/Public direct includes | Engine/Private direct includes | Tests/Tools/Runtime/Server/Editor/Apps direct includes |
| --- | --- | --- | --- | --- |
| `SagaEngine/Render/RenderGraph/RGTypes.h` | None | `RenderGraph.h`, `RGPass.h` | `GBufferPass.h`, `LightingPass.h` | None |
| `SagaEngine/Render/RenderGraph/RGPass.h` | None | `RenderGraph.h` | `RGCompilation.h` | None |
| `SagaEngine/Render/RenderGraph/RGCompilation.h` | None | None | `RenderGraph.cpp`, `RGCompilation.cpp` | None |
| `SagaEngine/Render/RenderGraph/RenderGraph.h` | None | None | `RenderGraph.cpp` | None |
| `SagaEngine/Render/RenderPasses/GBufferPass.h` | None | None | `LightingPass.h`, `RenderPass.cpp` | None |
| `SagaEngine/Render/RenderPasses/LightingPass.h` | None | None | `RenderPass.cpp` | None |
| `SagaEngine/Render/RenderPasses/PostProcessGraph.h` | None | None | None | None |
| `SagaEngine/Render/RenderPasses/ShadowMap.h` | None | None | None | None |

## Symbol Usage Summary

`RenderGraph` implementation is private and currently uses the public
RenderGraph headers as its concrete implementation surface:

- `RenderGraph.cpp` implements `RenderGraph` resource registration, pass
  registration, compile, and execute behavior.
- `RGCompilation.cpp` implements `RGCompiler` and `CompiledGraph` scheduling.
- `RenderPass.cpp` implements `GBufferPass` and `LightingPass` stubs.

No direct usage of `PostProcessGraph.h` or `ShadowMap.h` was found in the
current source tree. Those headers are declarative pass/config data and may be
intended for editor, asset, cvar, or renderer configuration use later, but that
role is not established by current direct includes.

`RGTypes.h` depends on public `SagaEngine/RHI/Types` descriptors, and
`RenderGraph.h` exposes execution through `RHI::IRHI&` callbacks. That makes
the cluster partly backend-facing, but not yet a stable custom render extension
contract.

## File-Level Decision Matrix

| Header | Classification | Recommended action | Risk | Reason |
| --- | --- | --- | --- | --- |
| `SagaEngine/Render/RenderGraph/RGTypes.h` | Keep Public Internal for now | Move only with a render boundary plan | Medium | It anchors RenderGraph resource IDs/descriptors and is included by public RenderPasses headers. It also exposes public `RHI/Types`, so moving it alone would force a larger cluster move or facade strategy. |
| `SagaEngine/Render/RenderGraph/RGPass.h` | Internal render graph detail | Move private after boundary plan | Medium | Exposes pass scheduling/resource usage and `std::function<void(RHI::IRHI&)>` execution callbacks, which are graph implementation details rather than stable SDK API. |
| `SagaEngine/Render/RenderGraph/RGCompilation.h` | Internal render graph detail | Moved private | Low | Exposes compiler/schedule output used by `RenderGraph` internals; now included only from private render implementation. |
| `SagaEngine/Render/RenderGraph/RenderGraph.h` | Facade candidate with backend-facing leakage | Design render extension facade first | High | It looks like a possible future custom render extension API, but currently exposes concrete graph internals and execution callbacks over `RHI::IRHI&`. |
| `SagaEngine/Render/RenderPasses/GBufferPass.h` | Built-in render pass implementation detail | Moved private | Low | Built-in deferred pass stub used by private renderer implementation and `LightingPass.h`; no external direct includes. |
| `SagaEngine/Render/RenderPasses/LightingPass.h` | Built-in render pass implementation detail | Moved private | Low | Built-in deferred pass stub used by private renderer implementation; no external direct includes. |
| `SagaEngine/Render/RenderPasses/PostProcessGraph.h` | Public-internal renderer configuration debt | Keep public-internal until extension/config boundary exists | Medium | Declarative post-processing chain data mentions editor/cvar serialization use, but no current direct includes prove it is a stable editor/tooling extension point. |
| `SagaEngine/Render/RenderPasses/ShadowMap.h` | Public-internal renderer configuration debt | Keep public-internal until extension/config boundary exists | Medium | Declarative shadow pass/allocation data may become renderer configuration or tooling-facing data, but no current direct includes prove a stable public contract. |

## Area Assessment

`SagaEngine/Render/RenderGraph` is mostly implementation-only renderer
infrastructure today. It is not currently used by external modules or tests by
direct include. However, `RenderGraph.h` has facade-like shape and `RGTypes.h`
is shared by public pass headers, so moving individual headers without a render
extension boundary would be brittle.

`SagaEngine/Render/RenderPasses` is split:

- `GBufferPass.h` and `LightingPass.h` were implementation-only built-in pass
  stubs and have been moved private.
- `PostProcessGraph.h` and `ShadowMap.h` are declarative render configuration
  surfaces. They are not proven external API, but they explicitly describe
  editor/cvar/config-style use and should not be moved until that boundary is
  designed.

The current evidence does not show a stable custom render extension API,
backend API, or editor/tooling extension point for these headers. The safer
classification is temporary public-internal debt pending a narrower render
extension/config facade.

The follow-up
[Engine Render Extension / Config boundary contract](ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md)
records that conservative boundary direction as a config-first public surface,
not a stable SDK or custom pass API claim.

The later RenderPipelineConfig shell phase added
`SagaEngine/Render/RenderPipelineConfig.h` as a narrow public config DTO
header. It does not move or rename any RenderGraph or RenderPasses headers, and
it does not make the audited cluster a stable render SDK.

The following low-risk migration moved `RGCompilation.h`, `GBufferPass.h`, and
`LightingPass.h` private without adding forwarding public headers. The
remaining public-internal render debt is `RenderGraph.h`, `RGPass.h`,
`RGTypes.h`, `PostProcessGraph.h`, and `ShadowMap.h`.

The subsequent safe-move migration moved `FrameGraphExecutor.h`,
`CommandBuffer.h`, `CommandRecorder.h`, and `Renderer.h` private without adding
forwarding public headers. Frame execution and command recording are no longer
part of the public render include surface.

The focused
[Engine Render residual public surface audit](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md)
classifies the remaining public Render headers after those safe moves.

## Recommendation

Stop and design a render extension/config facade first.

Do not immediately move all RenderGraph headers together. `RenderGraph.h`
exposes concrete graph internals and backend execution callbacks, while
`RGTypes.h` is still required by public RenderPasses headers. Moving the full
RenderGraph cluster now would either leave public RenderPasses depending on
private graph types or force a larger RenderPasses move before the config
boundary is clear.

Do not move all RenderPasses first. `GBufferPass.h` and `LightingPass.h` are
low-risk, but `PostProcessGraph.h` and `ShadowMap.h` are declarative
configuration candidates that need a render config/editor/tooling boundary
decision.

Safest next implementation sequence:

1. Design a narrow render extension/config boundary that separates public
   renderer configuration from private graph scheduling and built-in pass
   implementation.
2. Keep `GBufferPass.h`, `LightingPass.h`, and `RGCompilation.h` private after
   the low-risk implementation slice.
3. Keep frame execution, command recording, and the old renderer shell private
   after the safe-move implementation slice.
4. Reassess `RenderGraph.h`, `RGPass.h`, `RGTypes.h`, `PostProcessGraph.h`, and
   `ShadowMap.h` through the focused
   [Engine Render residual public surface audit](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md)
   before any further migration.

## Status

Report only. This audit does not fix the Render public/private boundary and
does not claim a stable render SDK or extension API.

The related
[Engine Render Extension / Config boundary contract](ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md)
documents the recommended config-first boundary before any RenderGraph or
RenderPasses migration.

The RenderPipelineConfig shell is now the first public config-owned DTO surface
for that boundary. The RenderGraph and RenderPasses headers remain
public-internal debt until a later migration slice, except for the completed
private move of `RGCompilation.h`, `GBufferPass.h`, `LightingPass.h`,
`FrameGraphExecutor.h`, `CommandBuffer.h`, `CommandRecorder.h`, and
`Renderer.h`.

See the
[Engine Render residual public surface audit](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md)
for the current classification of the remaining public Render headers.
