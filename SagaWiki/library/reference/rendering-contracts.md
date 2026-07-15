---
title: Rendering and graphics contracts
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Rendering and graphics contracts

This reference describes the current public vocabulary, private backend ownership, frame path, coordinate convention, material and lighting boundaries, world-interest input, and evidence levels for SagaEngine graphics. It is a contract map, not a claim that the renderer or external graphics SDK is finished.

## Ownership split

`Engine/Source/Runtime/RHI` owns the lower-level graphics abstraction: vendor-neutral handles and descriptions, device-facing operations, binding validation, frame-resource vocabulary, backend selection, and private native backend implementation. `Engine/Source/Runtime/Render` owns scene-facing policy: views, cameras, render entities, frame snapshots, culling, LOD, materials, render graph vocabulary, pipeline configuration, world interest, and submission to an `IRenderBackend`.

The private Diligent implementation spans the intended RHI/Render backend owners. Native device, immediate context, swapchain, pipeline, texture, buffer, descriptor/binding, capture, and synchronization details do not become public contract merely because a test needs to inspect them.

Programs select configuration and host a loop. They do not create an independent Diligent device lifecycle. UI and editor modules can submit engine-level render data, but they do not own native swapchain lifetime unless an explicit host integration contract delegates it.

## Public graphics vocabulary

The public RHI/graphics surface uses backend-neutral concepts:

- opaque buffer, texture, shader, pipeline, view, and sampler handles;
- resource descriptions and usage/access flags;
- pipeline rasterizer, depth/stencil, blend, topology, and shader descriptions;
- binding layouts, binding values, validation results, and fallback status;
- frame resource classes and budget/status snapshots;
- backend request, capability, health, failure, and diagnostic values.

Opaque handles separate identity from native pointers. A handle can be generation-aware so a destroyed slot cannot be mistaken for a newly created object at the same index. Public query APIs can report registered state and approximate logical accounting without exposing a Diligent interface pointer.

Registration or CPU validation is not the same claim as native GPU allocation. A resource description can validate, receive a handle, and appear in a registry while a specific path remains CPU-only. Documentation and tests must state whether evidence reaches actual native creation, upload, binding, draw, or pixel output.

### Target and install boundary

The public graphics target/export exposes only vendor-neutral include and link requirements. Private graphics implementation can depend on the canonical Diligent runtime target. Installed headers/targets must not include or ship Diligent/native SDK headers as if they were SagaEngine API, and an installed consumer must not need private source roots.

Third-party license material can be installed for an artifact without installing the vendor's development headers or backend target. License inclusion and public API exposure are separate decisions.

## Public render vocabulary

The public Render surface describes engine concepts:

- camera, frustum, view, and frame context;
- render entity and render-world input;
- deterministic frame snapshots and draw packets;
- culling and LOD policy;
- material and mesh asset values;
- material graph and runtime build contracts;
- render pipeline and feature configuration;
- render streaming/residency decisions;
- backend interface/factory and submission results.

Some current public headers remain broader than an ideal stable SDK. Their presence records in-tree contract or transitional public-internal debt, not a promise of permanent binary compatibility. Concrete graph compilation, command recording, built-in pass classes, native resource owners, device services, capture, caches, and backend frame execution belong private.

## Config-first extension boundary

Render customization starts with declarative configuration rather than arbitrary native callbacks. `RenderPipelineConfig` and supporting option/status structures can express desired features, quality, post-processing, shadow, output, and budget settings. The private renderer validates and translates those values into its graph and backend operations.

A future custom-pass or plugin boundary would require explicit lifecycle, ordering, resource declaration, validation, compatibility, thread, backend-access, and failure rules. Naming a registry today would imply those rules already exist. Current code must not expose `IRHI` or a native command list to a consumer callback simply to create a quick extension seam.

Allowed public extension data is copied, stable, and engine-owned. Forbidden extension data includes mutable graph compiler state, transient resource lifetime internals, backend caches, native descriptor objects, concrete pass implementation objects, or ownership of presentation.

## Canonical coordinate convention

SagaEngine graphics uses a right-handed coordinate system:

- `+X` points right;
- `+Y` points up;
- camera/world forward is `-Z`;
- clip-space depth is zero at the near plane and one at the far plane;
- counter-clockwise winding is front-facing under the default convention;
- projection/view/model multiplication follows the current math contract;
- normal transformation uses inverse-transpose semantics when non-uniform transforms require it.

Texture and shadow coordinate conversion must have one owner. A backend adjustment such as vertical inversion is applied exactly once. CPU culling, projection math, mesh import, shader transforms, back-face culling, depth tests, frame capture, and shadow projection must agree. A platform adapter may translate the convention for a native API, but it does not redefine engine space.

## Backend selection and capability reporting

Configuration expresses a preference and acceptable fallback. The private factory selects among compiled and available backends in a deterministic order. The request value is not proof that every named backend is present, tested, or equivalent.

Capability state distinguishes:

- not requested;
- requested and available;
- requested but unavailable/unsupported;
- disabled by selected profile or safe fallback.

Capabilities should be conservative. A feature is not advertised because the vendor API could theoretically support it. It is advertised when the selected engine path implements and validates it under the declared backend. Health/failure values distinguish unavailable backend, initialization failure, invalid configuration, minimized/skip conditions, and later runtime failure without exposing native error objects as public ABI.

Capability fallback is deterministic. Required capability failure blocks initialization or the requesting workflow. Optional capability failure selects a declared lower-quality/disabled path and reports it. It never silently maps a requested native API to another while continuing to report the requested API as active.

## Native lifecycle

`DiligentDeviceFactory.cpp` is the singular private native device/context/swapchain creation owner in the current layout. The resulting services are passed through private backend contracts. No Render extension, program, test fixture, or UI adapter creates a second independent native lifecycle for normal engine rendering.

The lifecycle order is:

1. validate backend request and platform/window input;
2. create the native device/context and presentation objects;
3. establish fallback resources, bindings, frame slots, and backend caches;
4. initialize Render resources and higher-level pipelines;
5. execute frames;
6. stop new submissions and drain owned work;
7. release render/native resources in dependency order;
8. destroy presentation and device services.

Normal frame flow does not call global `WaitForIdle`. Per-frame synchronization and frame-slot/timeline ownership preserve throughput and correctness. Full idle waits are reserved for teardown or a diagnostic capture/recovery action that explicitly requires them.

## Frame lifecycle

The high-level backend contract follows begin, submit, and end/present semantics. `BeginFrame` establishes the current render target/frame slot and resets per-frame diagnostics. Render constructs or receives a `RenderFrameSnapshot`, culls and orders visible items, builds draw submissions, and submits through the backend. `EndFrame` finalizes/presents and advances lifetime tracking.

An invalid phase transition is diagnosed: submit before begin, a second begin, end without begin, stale resource submission, or a submission incompatible with the active frame configuration is rejected. The renderer must not continue by drawing with undefined old state.

Frame data is snapshot-like. Simulation and replication do not expose mutable ECS storage to an asynchronous renderer. A snapshot carries the view family, visible/draw data, stable resource handles, and diagnostics/counters needed for that frame. This establishes a possible render-thread boundary without claiming a dedicated render thread is currently complete.

## Render world and interest

`RenderWorld` is the rendering representation of scene-facing entities. It does not own gameplay authority. Extraction maps relevant simulation/client state into render entities and frame packets. Removing a render entity does not delete the authoritative entity; an authoritative entity can exist while it is not rendered.

Four decisions remain separate:

| Decision | Question |
| --- | --- |
| Network relevance | Should data cross the network for this client? |
| Client/world streaming relevance | Should the client retain/load this world cell or entity? |
| Render visibility | Does the item contribute to this view after frustum/occlusion/policy? |
| Resource residency | Which mesh LOD or texture mip bytes/device resources are needed? |

One can inform another, but they are not aliases. The renderer can request resources based on view distance and material importance. It may not promote visibility into server authority or persistence truth.

## Culling, LOD, and residency

CPU-side culling uses the canonical view/frustum contract and produces deterministic decisions for the same snapshot. LOD selection considers distance, projected importance, asset availability, and configured policy. Missing ideal data results in a declared fallback or omission with diagnostics, not an out-of-range resource access.

`RenderStreamingResidency` interprets which texture mip or geometry LOD should be requested. The `Resources` module remains owner of byte streaming and generic residency. Render owns texture/geometry pool policy and GPU lifetime. A resource request records priority and fallback reason so diagnostics can explain visible degradation.

Texture and geometry budgets are estimates for the allocations the owner can measure. CPU source bytes, decoded objects, staging memory, and native GPU allocations are reported separately when possible. A logical registry byte count must not be described as exact physical GPU memory.

## Materials

A material contract separates authoring graph, compiled/derived representation, runtime parameters, and native pipeline/binding state. The authoring graph is project source or editor model. Generated shader/material artifacts are derived and freshness-checked. Runtime material build validates parameter kinds, resource references, shader/pipeline requirements, and fallback behavior. Private RHI code creates native objects.

Material graph categories can include constants, parameters, texture sampling, math, vector operations, coordinate inputs, and output nodes only where the current contract supports them. A node appearing in an authoring descriptor does not prove a complete shader compiler or every combination. Unsupported nodes or connections produce diagnostics rather than silently changing the material.

Parameter overrides identify the parameter and expected kind. Missing resources can use explicit engine fallback bindings; kind mismatch and invalid handles remain errors. Fallback validation is not proof that native descriptors were created or that a draw bound them.

## Lighting and shadows

The evidenced private forward opaque path includes a bounded model: unlit and lit-diffuse shading, directional light and ambient contribution, vertex-normal Lambert response, and a directional shadow-map path on the tested Diligent/Vulkan configuration. The minimum conceptual equation is:

```text
ambient = albedo * ambientColor * ambientIntensity
diffuse = albedo * lightColor * lightIntensity * max(dot(normal, -lightDirection), 0)
final   = ambient + shadowVisibility * diffuse
```

Light direction is the direction rays travel; the surface-to-light vector is its negation. Unlit materials ignore directional and shadow contribution. Ambient remains separate so a surface without positive diffuse can remain visible according to configured ambient intensity.

Shadow resources, depth pass, sampling, bias, filtering, projection, and capture diagnostics are private implementation. Current evidence for one directional orthographic shadow map must not be expanded into a claim for cascades, point/spot shadows, transparent/skinned/terrain shadow completeness, cross-vendor depth sampling, or production performance.

Color space is likewise explicit. A procedural fixture using linear-valued RGBA bytes does not prove asset-driven sRGB decode or a full color-managed pipeline. Swapchain sRGB format and source texture decode are separate decisions; shaders must not accidentally apply a second gamma conversion.

## Render graph boundary

The render graph can describe resource uses, passes, dependencies, compilation order, and deterministic dumps. Validation detects duplicate declarations, invalid resource references, incompatible uses, cycles, missing writers/readers where required, and execution of dirty/uncompiled state.

CPU graph compilation and callback order do not by themselves prove native GPU execution, transient aliasing, framebuffer creation, descriptor binding, or synchronization. Private executors bridge graph output to command recording only where implemented and tested. Built-in G-buffer, lighting, shadow, and post-process implementation types remain private even if their configuration is public.

A graph mutation invalidates compiled state. Execution uses an immutable compilation snapshot or refuses until recompiled. Diagnostic dumps are derived evidence; editing a dump does not edit the graph.

## Resource handles and destruction

Every submission validates that mesh, material, texture, buffer, shader, and pipeline handles are live and compatible. Generation-aware identity prevents use-after-destroy when a slot is reused. Destruction observes in-flight frame ownership; a native resource is not freed while an earlier submitted frame may still reference it.

Frame-slot tracking and GPU timeline/fence logic are private. The public promise is that a successful destroy request follows the documented lifetime and that later stale submissions are rejected. A shutdown leak summary can report registered resources that outlived expected ownership without exposing vendor objects.

### Binding contracts

A binding layout declares required/optional slots, stage visibility, and expected resource kind. A binding set supplies handles. Validation rejects missing required resources, wrong kind/usage, destroyed or stale generation, layout/pipeline incompatibility, and unsupported slots.

Optional missing texture/buffer/sampler can resolve to a canonical fallback where policy defines one. An explicitly supplied stale handle must not be replaced silently by fallback; that would hide use-after-destroy. Cache keys include pipeline/layout/resource generations so a reused numeric slot cannot reuse an old native binding record.

CPU binding validation and fallback planning are separate from native descriptor/SRB creation and draw binding. GPU tests are required for the latter claim.

## Diagnostic frame capture

Frame capture is a private diagnostic/testing path. It copies the current color target to CPU-readable staging at the defined point after submission and before presentation, waits only as required for that capture, maps supported formats, and normalizes output for comparison.

Capture is not part of the general installed render interface. Its blocking behavior is not acceptable as a normal frame synchronization strategy. A capture test proves the configured pixel path under its GPU/driver/backend environment, not identical results on all backends.

## Evidence levels

Rendering evidence has distinct levels:

- compile/public-header evidence proves vendor-neutral headers can be consumed;
- architecture evidence proves dependency, private backend, target, and install boundaries;
- CPU unit evidence proves math, graph validation, culling, LOD, handles, and deterministic reports;
- null-backend evidence proves lifecycle/state handling without native pixels;
- GPU integration evidence proves native creation, upload, draw, depth, texture, resize, lighting, shadow, or capture for a stated backend/environment;
- interactive evidence proves a human-observed scenario and controls, not an automated pixel invariant;
- performance evidence requires measured workload and environment; ordinary correctness tests do not establish it.

Pixel tests should compare robust properties: non-clear coverage, relative luminance, expected occlusion, texture quadrant mapping, or movement of a shadow. Exact byte equality across GPUs is appropriate only where the contract genuinely requires it.

### Current GPU evidence map

The current Diligent tests contain focused evidence for lifecycle/reinitialize/resize, native buffers/textures/samplers/shaders/pipelines, constant/texture/sampler bindings and cache invalidation, stale/destroyed handle rejection, coordinate/depth/winding/UV behavior, overlay draw/scissor/alpha/order/lifetime/thread rejection, directional lighting, shadow behavior, and a named StarterArena pixel frame.

Each test proves its asserted fixture on the executing GPU environment. A skipped no-display/no-device test remains unavailable evidence. A multi-frame stress-named GPU test remains GPU integration behavior and should not be conflated with the repository's separate StressTests acceptance.

## Current non-claims

The current contracts do not establish a stable external graphics SDK, public native backend API, arbitrary custom pass/plugin API, complete PBR stack, all light and shadow types, complete post-processing, HDR/tonemapping, virtual texturing, full terrain/character/VFX rendering, universal backend parity, device-loss recovery, dedicated render thread, or production performance certification.

They also do not establish that every public Render header is final. Transitional public-internal types should be narrowed through deliberate consumer and install review rather than preserved with compatibility aliases solely because the hard cutover moved them.

## Change checklist

Before changing graphics/render behavior:

- identify RHI versus Render ownership;
- keep Diligent/native objects private;
- preserve the singular device lifecycle;
- state whether the change is registration, CPU validation, native allocation, binding, draw, or pixel evidence;
- apply coordinate/backend adjustment exactly once;
- keep network, world, render, and residency relevance distinct;
- define handle and in-flight destruction behavior;
- add focused CPU, architecture, installed-consumer, or GPU evidence at the correct level;
- update non-claims when a new slice becomes real, without advertising adjacent unfinished work.
