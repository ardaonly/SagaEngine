# Graphics Render Pipeline Hardening Plan

> **Status:** Current architecture boundary document
> **Scope:** Graphics and render pipeline ownership, boundaries, evidence, and non-claims
> **Audience:** SagaEngine maintainers, reviewers, tooling agents, and future renderer contributors
> **Not a backlog:** Implementation ordering belongs in active work plans. This document defines the architectural rules those plans must obey.

---

## 1. Purpose

This document defines the current architecture constraints for SagaEngine's
graphics and render pipeline.

It does not replace implementation worklists. It does not define milestone order. It does not claim renderer completion.

Its purpose is to answer four questions:

1. Which layer owns each graphics/render responsibility?
2. Which dependencies are allowed to cross public, private, backend, runtime, and server boundaries?
3. Which evidence is required before a renderer claim may be made?
4. Which claims are explicitly forbidden until proven by implementation and tests?

Active work plans may contain short-term slice order, temporary notes, and
current "what next?" decisions. They must not become the only source for
long-term renderer architecture decisions.

---

## 2. Current Claim Level

SagaEngine currently has a graphics/render foundation under active hardening.

The allowed claim is:

```text
SagaEngine has a vendor-neutral graphics API foundation and a private Diligent-backed
graphics implementation path, with partial resource, capability, binding, frame-resource,
and RenderGraph validation coverage.
```

The following claims are not allowed:

```text
SagaEngine has a production renderer.
SagaEngine has a stable external graphics SDK.
SagaEngine has complete native GPU resource/upload coverage.
SagaEngine has a complete material, shader, lighting, post-process, or scene renderer stack.
SagaEngine has a complete RenderGraph-to-Diligent execution bridge.
```

When a subsystem is described as `foundation`, `entry`, `partial`, or `validation`, the evidence scope must be stated. A CPU-side validation path is not the same as native GPU execution.

---

## 3. Source-of-Truth Relationship

This document is the high-level boundary map for render architecture. Narrower
subsystem contracts may own more specific rules.

Official architecture documents must own:

- public/private API rules,
- dependency boundaries,
- backend ownership,
- render/runtime integration boundaries,
- evidence requirements,
- forbidden claims.

If an active work plan and this document disagree, this document wins for
architecture boundaries. Work plans may still own immediate local task order.

---

## 4. Layer Ownership

### 4.1 `Vendor/Diligent`

`Vendor/Diligent` contains third-party graphics infrastructure controlled as a vendored dependency.

Rules:

- Diligent is not Saga-owned renderer architecture.
- Diligent is not exposed as SagaEngine public API.
- Diligent may be patched, pinned, documented, and integrated, but it remains vendor code.
- Diligent ownership metadata must remain separate from Saga-owned source.

Allowed responsibilities:

- low-level graphics API abstraction,
- device/swapchain/native resource backend work,
- backend-specific implementation details.

Forbidden responsibilities:

- defining Saga public graphics vocabulary,
- defining Saga render graph semantics,
- defining Saga material/shader/scene architecture,
- leaking native handles into public headers.

---

### 4.2 `SagaGraphics` Public API

`SagaGraphics` is the public, vendor-neutral graphics surface.

It owns Saga-facing vocabulary such as:

- backend descriptions,
- capability descriptors,
- resource descriptors,
- opaque resource handles,
- frame/resource status types,
- public diagnostics objects,
- public query objects that do not expose native handles.

Public graphics API must remain Saga-owned.

Forbidden public API leaks include:

```text
Diligent::
Diligent*
Vk*
Vulkan
ID3D*
D3D*
MTL*
Metal
TheForge
native resource pointer types
```

Backend names may appear only as intentionally documented policy vocabulary when approved by the public API contract. Native type names and vendor object types remain forbidden.

---

### 4.3 `SagaGraphicsPrivate`

`SagaGraphicsPrivate` owns implementation-side graphics behavior.

It may depend on private backend targets and private implementation files.

Allowed responsibilities:

- resource registries,
- private resource slot storage,
- private adapter logic,
- internal capability mapping,
- backend dispatch,
- lifecycle handling,
- CPU-side diagnostics and validation.

Forbidden responsibilities:

- exposing Diligent include paths through public targets,
- making server/headless targets depend on graphics backend targets,
- turning private implementation details into public SDK obligations.

---

### 4.4 Diligent Backend Adapter

The Diligent backend adapter lives under private graphics/backend ownership.

Allowed responsibilities:

- Diligent device/swapchain/frame lifecycle,
- backend initialization and shutdown,
- backend status diagnostics,
- private native resource creation when implemented,
- backend capability probing when implemented,
- backend-specific failure handling.

Required boundary:

```text
Public API -> Saga-owned descriptors/handles/status
Private backend -> Diligent/native objects
```

Forbidden boundary:

```text
Public API -> Diligent/native objects
Game/runtime entity -> Diligent/native object
Server/headless target -> Diligent backend dependency
```

---

## 5. RenderGraph Ownership

RenderGraph is a Saga-owned render dependency and validation model. Native
execution integration is a later boundary.

It owns:

- pass declarations,
- resource declarations,
- read/write dependencies,
- validation diagnostics,
- deterministic graph dumps,
- CPU-side execution/report snapshots where implemented,
- future execution bridge contracts.

RenderGraph must not be defined as a Diligent feature.

Diligent may execute a compiled graph later, but Diligent must not own the public graph model.

Allowed current claims:

```text
RenderGraph validation and deterministic diagnostic coverage exist where tested.
CPU-side execution/report snapshots may exist where implemented.
RenderGraph mutation after compile is guarded where tested so stale compiled
CPU-side pass lists are not executed.
```

Forbidden current claims:

```text
RenderGraph is fully connected to native frame execution.
RenderGraph owns a complete transient resource allocator.
RenderGraph owns material/shader/lighting/post-process execution.
RenderGraph is production optimized.
```

---

## 6. Resource Model Boundary

Saga graphics resources must be represented through Saga-owned descriptors and opaque handles.

Current foundation may include:

- descriptor validation,
- generation-aware handles,
- stale handle rejection,
- invalid/double-destroy behavior,
- debug names,
- registered-resource query results,
- approximate logical memory accounting,
- liveness/leak summaries,
- initial data shadow-copy contracts.

These are not equivalent to complete native GPU resource support.

Native GPU object creation, upload/staging, GPU memory residency, driver allocation behavior, and draw-ready resource integration require separate implementation and evidence.

Rules:

- Public resource handles must not expose native pointers.
- Query APIs must not return backend object addresses.
- Approximate logical memory must not be described as exact GPU memory.
- Shadow-copied initial data must not be counted as proof of native upload.
- Resource liveness summaries are diagnostics; they are not native GPU leak proof unless native backend evidence exists.

---

## 7. Binding and Frame Resource Boundary

Binding and frame-resource concepts must be Saga-owned.

Allowed current foundation includes:

- CPU-side binding vocabulary,
- CPU-side binding validation,
- CPU-side binding layout conformance for unexpected binding slots,
- CPU-side fallback binding policy that validates missing texture, buffer, and
  sampler fallback handles without creating native descriptors or GPU objects,
- frame resource class vocabulary,
- CPU-side frame allocation snapshots,
- basic frame-resource memory/budget counters.

This does not prove:

- native descriptor set implementation,
- GPU uniform ring buffer,
- upload heap,
- descriptor heap allocator,
- draw binding integration,
- bindless implementation,
- async compute support,
- GPU-driven submission.

Rules:

- Public binding vocabulary must not be modeled directly after Diligent object types.
- CPU-side validation, including fallback binding policy, must be labeled as
  CPU-side validation.
- Native descriptor ownership must remain private when implemented.
- Frame-in-flight ownership vocabulary is a CPU-side ownership/budget contract;
  native multi-frame resource mutation and no-submit policy require separate
  evidence before they are claimed.

---

## 8. Shader, Material, and Asset Pipeline Boundary

Shader, material, and render asset cooking systems must remain Saga-owned architecture.

Diligent may compile or create backend shader objects through private adapters, but Saga must own:

- shader source descriptions,
- shader stages,
- variant keys,
- include policy,
- reflection schema,
- cache policy,
- material asset and instance vocabulary,
- material graph/authoring contracts,
- cooked render asset manifests.

Forbidden design:

```text
Material system is designed directly around Diligent public objects.
Shader pipeline exposes Diligent-native shader objects as public API.
Runtime renderer depends on raw asset paths as its primary model.
```

Allowed future direction:

```text
Saga-owned shader/material/cooking contracts feed private backend creation.
```

No shader/material/cooking claim is valid without tests for diagnostics, determinism, missing inputs, and compatibility behavior.

---

## 9. Runtime, Server, and UI Boundary

Graphics/rendering is a runtime client concern, not a server authority requirement.

Rules:

- Server/headless targets must not require Diligent.
- Server authority must not depend on renderer state.
- Game/runtime entities must not store native graphics resources directly.
- Render visibility must not be treated as network interest.
- UI is outside this graphics/render pipeline document unless a separate UI/render integration contract explicitly says otherwise.

The renderer may consume world snapshots, scene extraction output, camera/view data, and render-interest hints. It must not become the source of truth for simulation, persistence, authority, or networking.

---

## 10. World, Visibility, and MMO Boundary

Saga's renderer must be designed for large persistent worlds, but this document does not claim those systems are complete.

Architectural rules:

- Render visibility and network interest are separate concepts.
- World partitioning and render chunking must be contractually separated from gameplay ownership.
- Culling, LOD, HLOD, impostors, and streaming must be measurable.
- Texture and geometry residency must have budget and diagnostic paths before large-world claims are made.
- Avatar/equipment rendering is a first-class MMO renderer concern when character rendering begins.
- Terrain, vegetation, sky, weather, and water strategies are renderer-domain systems, not proof of production open-world rendering.

Forbidden claim:

```text
Open-world/MMO rendering is complete because a backend can draw frames.
```

Required claim shape:

```text
Specific world/render contracts are implemented and covered by named tests.
```

---

## 11. Diagnostics and Evidence

A render claim is valid only when its evidence scope is explicit.

Recognized evidence types:

- architecture boundary tests,
- public forbidden-token scans,
- public header compile smoke tests,
- install surface scans,
- CMake target dependency checks,
- unit tests for deterministic CPU-side behavior,
- GPU smoke tests when native execution is claimed,
- golden image tests when visual regression is claimed,
- memory/residency reports when budget behavior is claimed,
- crash/telemetry artifacts when field diagnostics are claimed.

CPU-side evidence is valid, but it must not be represented as GPU execution evidence.

Report-only evidence is useful, but it does not close a boundary unless the corresponding guard fails on regression.

---

## 12. Required Boundary Guards

The graphics/render pipeline must preserve these guard families:

```text
Public API vendor/native token guard
Public umbrella header compile smoke
Install surface guard
Private include boundary guard
Graphics target dependency guard
Server/headless renderer dependency guard
Capability fallback tests
Resource handle/lifetime tests
Binding validation tests
Frame-resource accounting tests
RenderGraph validation/dump tests
```

A subsystem may be called closed only if its guard exists and is enforced.

If a guard is report-only, the claim must say report-only.

---

## 13. Maturity Vocabulary

Use precise maturity language.

Allowed terms:

```text
entry
partial
validation-entry
foundation
report-only
guarded
implemented-unverified
current
planning direction
historical draft
```

Rules:

- `foundation` means a tested base exists, not a complete subsystem.
- `entry` means a narrow first slice exists.
- `partial` means known required behavior is still missing.
- `guarded` means regression is blocked by an enforced test or script.
- `report-only` means the issue is visible but not yet enforced.
- `implemented-unverified` means code exists without sufficient proof.

Forbidden misuse:

```text
foundation == complete
validation == execution
CPU-side == GPU-backed
registered handle == native resource
capability vocabulary == runtime native probe
graph dump == render execution bridge
smoke test == production readiness
```

---

## 14. Non-Goals

This document does not approve or claim:

- a production renderer,
- a stable external graphics SDK,
- a complete RenderGraph execution bridge,
- complete Diligent/native resource creation and upload,
- a complete material system,
- a complete shader pipeline,
- lighting/shadow/post-process completion,
- complete texture or geometry streaming,
- complete asset cooking,
- production open-world renderer readiness,
- UI integration,
- engine-wide package/editor/product claims.

---

## 15. Hardening Priorities

Implementation order remains outside this document, but hardening must preserve this dependency logic:

1. Public/vendor boundary before public API expansion.
2. Resource lifetime and diagnostics before native upload claims.
3. Binding/frame-resource vocabulary before descriptor heap claims.
4. RenderGraph validation before backend execution bridge claims.
5. Capability matrix before feature enablement claims.
6. Shader/material ownership contracts before material authoring claims.
7. Asset cooking contracts before runtime raw-asset independence claims.
8. Runtime/server/headless separation before renderer integration claims.
9. World/render-interest contracts before MMO/open-world rendering claims.
10. Diagnostics and quality gates before maturity claims.

This list is a dependency rule, not a milestone schedule.

---

## 16. Relationship to Other Documents

This document should be read with:

- `docs/architecture/ARCHITECTURE_OVERVIEW.md`
- `docs/architecture/CLAIM_AND_EVIDENCE_POLICY.md`
- `docs/architecture/SOURCE_OF_TRUTH_MAP.md`
- `docs/architecture/RENDER_PUBLIC_API_CONTRACT.md`
- `docs/architecture/GRAPHICS_TARGET_BOUNDARY_INVENTORY.md`
- `docs/architecture/GRAPHICS_BACKEND_PREFERENCE_ORDER.md`
- `docs/architecture/GRAPHICS_CAPABILITY_MATRIX_V0.md`
- `docs/architecture/ENGINE_RENDERGRAPH_PUBLIC_HEADER_AUDIT.md`
- `docs/architecture/ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md`
- `docs/architecture/ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md`
- `docs/architecture/ARCHITECTURE_BOUNDARY_HARDENING_PLAN.md`

If a detailed render document provides narrower rules for a specific subsystem, the narrower document applies to that subsystem. This document remains the high-level graphics/render boundary reference.

---

## 17. Final Rule

SagaEngine's renderer must be Saga-owned at the architecture level and backend-backed at the implementation level.

The correct model is:

```text
Saga-owned public graphics vocabulary
        -> Saga-owned resource, binding, frame, and RenderGraph contracts
        -> private backend adapters
        -> vendored Diligent/native implementation
```

The incorrect model is:

```text
Diligent objects become Saga public API.
Backend success becomes renderer maturity.
CPU validation becomes GPU execution proof.
Planning language becomes current product claim.
```

The renderer may grow toward a large-world, MMO-oriented, high-end renderer foundation. It must do so through explicit ownership boundaries, guarded evidence, and narrow claims.
