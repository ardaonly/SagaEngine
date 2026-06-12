# Render Public API Contract

> Last updated: 2026-06-07

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

R3C conservative capability matrix v0 adds scalar-only
`RenderBackendCapabilities` capability vocabulary, `RenderQualityPreset`, and
quality/fallback helpers. These report conservative baseline support from the
current shell; they do not perform native backend feature queries or emit
capability report artifacts.

R4 partial/foundation coverage adds generation-aware private slot registries
behind the existing opaque graphics handles for null and Diligent adapter
behavior. It also adds vendor-neutral descriptor validation, backend-global
create failure reporting, approximate logical memory reporting, creation-time
initial data shadow-copy validation, registered-resource liveness diagnostics,
debug-name propagation, public registered-resource query, and a shutdown-time
registered-resource leak summary. The Diligent adapter remains registered-only
for these resources. This does not create native GPU resources, upload data,
expose backend pointers, or claim native GPU allocation accounting. It does not create native Diligent GPU resources.

R4B private/CPU validation partial coverage adds vendor-neutral CPU-side
binding vocabulary/validation, layout conformance checks for unexpected
binding slots, registered-resource query integration, and CPU-side fallback
binding policy for missing texture, buffer, and sampler bindings. It also adds
a private CPU frame resource allocator with lifecycle and alignment diagnostics
and public frame resource vocabulary for persistent, per-frame transient, and
swapchain-sized resource classes plus CPU budget snapshots. The fallback policy
validates fallback handles only; it does not create fallback native descriptors
or GPU objects. It does not create native descriptor sets, GPU ring buffers,
upload heaps, native uniform buffers, native multi-frame resource mutation, or
draw binding integration.

R5A validation/contract coverage adds RenderGraph resource descriptor
diagnostics, pass-local validation, duplicate pass/resource-use diagnostics,
compiled-state invalidation after graph mutation, compile/execution snapshot
counts, and deterministic dump v1 while preserving the existing `Compile()`
bool and CPU-side callback execution model. The diagnostics, dump, dirty-state
guard, and execution snapshot coverage are deterministic CPU-side test coverage
only. It does not add a SagaGraphics execution bridge, Diligent execution
bridge, material system, shader compiler, lighting, or post-processing
pipeline. It also does not add a transient aliasing allocator, native
framebuffer/backbuffer pass, native descriptor set, upload heap, or GPU draw
binding.

R5 validation entry adds RenderGraph compile diagnostics; it remains a
validation/dump foundation entry, not a render execution bridge.

Playable Render Slice v0 adds a deterministic sandbox/test helper that builds
one procedural cube mesh, one checker texture, one opaque material, one render
entity, and one main camera. The slice verifies that this scene reaches
`RenderSubsystem` culling and backend `Submit()` as a real draw item. It uses
the existing `IRenderBackend` mesh/material/texture upload and submit seam; it
does not route through the SagaGraphics registered-only shell or claim a full
asset-driven scene renderer.

R6C/R6D CPU-side render residency foundation adds
`SagaEngine/Render/Streaming/RenderStreamingResidency.h` as a vendor-neutral
adapter over existing resource streaming vocabulary. It defines deterministic
texture mip residency decisions, material-importance and distance based
priority scoring, texture pool budget diagnostics, geometry LOD residency
decisions for static, skinned, and terrain chunk categories, missing mip and
missing mesh fallback diagnostics, stable request ordering, and byte-identical
residency report serialization. The Resources subsystem remains the owner of
asset byte streaming; this render contract interprets which mip or LOD should
be requested and what CPU-side fallback is acceptable.

R8/R8B/R8C/R11D CPU-side scene extraction and view-family foundation adds
vendor-neutral `RenderFrameSnapshot` / `RenderViewFamily` / draw packet
contracts plus a render interest/world streaming contract. These APIs turn
`RenderWorld` entities and view descriptors into deterministic CPU-side draw
packet snapshots, counters, and diagnostics. They also document that network
relevance, client streaming relevance, render visibility, and render resource
residency are separate concepts. This is a render-thread-compatible ownership
contract and diagnostics foundation; it is not a dedicated render thread,
multi-view render target implementation, terrain renderer, or GPU submission
bridge.

The high-level graphics/render pipeline boundary is documented in
[Graphics Render Pipeline Hardening Plan](GRAPHICS_RENDER_PIPELINE_HARDENING_PLAN.md).
The internal backend preference order is documented in
[Graphics Backend Preference Order](GRAPHICS_BACKEND_PREFERENCE_ORDER.md).
The conservative capability matrix is documented in
[Graphics Capability Matrix v0](GRAPHICS_CAPABILITY_MATRIX_V0.md).
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
It does not add RenderGraph execution, material, shader, lighting, or
post-processing behavior.
It does not complete R3B device-loss or swapchain recreation recovery.
It does not complete R3C native feature detection or capability artifacts.
It does not complete R4 native GPU resource creation, upload/staging, native
memory accounting, or resource diagnostics beyond registered-resource
lifecycle/liveness reporting.
It does not complete R4B native binding/frame resources.
It does not complete R5 RenderGraph execution.
It does not implement GPU texture upload.
It does not implement virtual texture residency.
It does not prove terrain or mesh streaming render smoke.
It does not implement a dedicated render thread.
It does not implement multi-view render targets.
It does not make network relevance the same thing as render relevance.
It does not turn the playable render slice into a production renderer,
asset-pipeline scene loader, shader compiler, lighting stack, or post-process
stack.
It does not claim `SagaEngine/Graphics` is a stable external SDK.

## Guardrails

Architecture tests must keep:

- `Graphics.h` umbrella compile smoke coverage;
- public `SagaEngine/Graphics` forbidden-token scan coverage;
- `SagaGraphics` target dependency checks;
- `SagaGraphicsPrivate` private target dependency checks;
- installed graphics include and backend/vendor target artifact scans.
