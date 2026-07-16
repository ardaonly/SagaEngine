---
title: Ownership and dependency contracts
status: current
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Ownership and dependency contracts

This reference records the post-cutover ownership model in enough detail to review source moves, public API changes, and target dependencies. The hard cutover retired broad legacy roots; it did not remove the need to audit whether a moved header is truly a public contract.

## Filesystem ownership

Runtime modules use:

```text
Engine/Source/Runtime/<Module>/
  Public/
  Private/
  Tests/
  CMakeLists.txt
```

Editor modules use the corresponding `Engine/Source/Editor/<Module>` shape. Programs use `Engine/Source/Programs/<Program>/Private` and should remain composition entry points. Permanent tools live under `Tools/<Tool>`; repository-internal checks live under `Tools/Developer/<Check>`; cross-cutting evidence lives under `Tests`.

Ownership means more than directory placement. The owning module is responsible for:

- semantics and lifecycle;
- public and private type placement;
- target source membership;
- dependency visibility;
- focused tests and diagnostics;
- install/export decisions;
- documentation of its stable contracts and explicit limitations.

## Layer direction

The durable direction is:

```text
Programs
  -> Editor modules (when the program is an editor surface)
  -> Runtime modules

Editor modules
  -> editor-neutral lower editor modules
  -> Runtime public contracts

Runtime composition
  -> domain runtime modules

Developer checks and tests
  -> inspected owners
```

Runtime modules do not depend on editor modules or programs. A low-level runtime module must not acquire an application dependency to reuse a convenience helper. Programs can configure modules but must not become the permanent owner of reusable engine behavior.

## Runtime owners

- Core and Math provide low-level engine-neutral foundations.
- Diagnostics owns structured fault, crash, health, memory, resource, and operational reporting.
- ECS and Simulation own entity/component mechanics and deterministic simulation state.
- Assets owns artifact/package manifest contracts and startup validation.
- Resources owns runtime asset identity, sources, registries, residency, and streaming.
- Input and Audio own their platform-neutral domain contracts; concrete platform/backend integrations remain implementation details.
- RHI owns device-facing abstraction and native backend integration; Render owns scene-facing rendering policy and composition.
- UI owns runtime UI contracts while concrete RmlUi integration remains a backend concern.
- Scripting owns runtime script lifecycle, binding, package validation, and C# host contracts.
- Networking owns transport-neutral networking and fault-injection policy; Replication owns snapshot/delta/interest and client replication behavior.
- Persistence owns storage contracts and concrete storage adapters.
- World owns cells, partitioning, relevance, world events, and a narrow public facade.
- ServerAuthority owns authoritative zones, actor ownership, command intake, and server-specific connection composition.
- SagaRuntime owns runtime composition rather than absorbing each domain implementation.

## Editor owners

- EditorCore owns commands, editor-neutral state, and basic contracts.
- EditorFramework owns shell, host, panels, layouts, docking, customization, and orchestration without owning Qt ABI.
- EditorQt owns Qt-specific application, widgets, and adapters.
- EditorAuthoring owns project inspection and semantic authoring workflows.
- VisualBlocksEditor publicly owns only the evidenced `Blocks/**` authoring and lowering contracts; read-only product descriptor generation remains private.
- `Engine/Source/Runtime/Scripting` owns script lifecycle and hosting; EditorAuthoring and VisualBlocksEditor own current editor-facing script inspection, projection, and patch workflows.
- EditorCollaboration owns the modern `SagaCollaboration` and `SagaShared` value/interface contracts; the legacy `SagaEditor::Collaboration` path is retired.
- EditorExperimental contains deliberately unstable editor work rather than promoting it as stable API.

## Public dependency visibility

A CMake dependency is `PUBLIC` only when a consumer compiling the owner's public headers must see that dependency or when the exported contract deliberately re-exports it. It is `PRIVATE` when implementation files alone require it. Tests link their own helpers and must not force private implementation dependencies into installed targets.

Review a dependency with these questions:

1. Does a public header include or name a type from the dependency?
2. Is that exposure intentional, vendor-neutral, and supported by installed-consumer tests?
3. Could an opaque handle, value record, interface, or factory remove the exposure?
4. Is a `PUBLIC` link compensating for a private include-path mistake?
5. Would the contract remain coherent if the concrete backend changed?

## Public header contract

Good public types include stable values, interfaces, configuration, results, events, diagnostics, opaque handles, and narrow factories. Warning signs include `Impl`, concrete vendor names, application-specific widgets, transport implementations, backend caches, global managers, or native lifecycle objects.

Public headers must not include another owner's `Private` path. Runtime public headers must not expose Qt. Public UI, Persistence, Input, Render, and RHI contracts should avoid direct RmlUi, pqxx/Redis, SDL, and Diligent implementation types. Qt exposure is permitted only through the explicit EditorQt contract and must not leak back into editor-neutral modules.

## Native graphics lifecycle

Diligent device, immediate context, swapchain, and native resource ownership remains private to the RHI/Render backend implementation. The factory/lifecycle owner is singular. Extensions and programs do not create a second device. Normal BeginFrame/submit/EndFrame flow does not use global `WaitForIdle`; idle waits belong to teardown or explicitly diagnostic capture paths.

This rule protects more than include cleanliness. Two native lifecycle owners can create mismatched devices, invalid resource destruction order, duplicated caches, or synchronization that only works under one executable.

## Aggregate and module targets

Each registered Runtime, Editor, and Product module compiles through an owner-named `Saga<Module>Module` OBJECT target. The compatibility targets `SagaEngine`, `SagaRuntimeLib`, `SagaServerLib`, `SagaEditorLib`, and `SagaProductLib` remain composition targets: they collect module objects and do not compile cross-owner raw source lists. A pure module target has:

- sources only from its owner directory;
- public includes rooted in that owner's `Public` area;
- private includes rooted in `Private`;
- public dependencies justified by public headers;
- deliberate install/export status;
- focused test dependencies separated from production consumers.

Module OBJECT targets are build-internal and are not installed or exported. Existing aggregate install/export names remain the consumer boundary. Specialized Diligent, SDL, Persistence/UI backend, and Collaboration targets keep their real owner sources rather than being forced into an unrelated aggregate.

### Target purity inventory

For each target, a useful review records:

- target name and semantic owner;
- every source directory outside that owner;
- public and private include roots;
- public, private, and test-only dependencies;
- compile definitions/options that escape to consumers;
- whether the target is installed/exported;
- which public headers the install contains;
- whether it is a module, program, backend, aggregate, interface shell, or test helper.

A target is pure when source ownership and dependency visibility match this inventory. An aggregate remains pure only while it composes registered objects and is prevented from accumulating raw cross-owner sources. A wrong-owner source should move/split through a focused build change rather than be normalized as permanent convenience.

Dependency visibility is reviewed both directions. A `PUBLIC` dependency can leak implementation; a mistakenly `PRIVATE` dependency can make installed consumers fail because a public header names its types. Passing in-tree builds do not settle the question because they can see extra include paths and linked objects.

## Program boundary

`SagaRuntime`, `SagaEditor`, `SagaEditorLab`, `SagaLauncher`, and `SagaSandbox` assemble modules for different workflows. There is no SagaServer program owner; `SagaServerLib` remains a ServerAuthority library target rather than an executable claim. None of these programs is permission to place reusable runtime/editor behavior in a program-private directory simply because only one executable currently calls it.

SagaLauncher may route project validation, script analysis, packaging, and program launch, but the owning tools remain authoritative. EditorLab and Sandbox are development/evidence surfaces. Deterministic checks should live in tests when possible; interactive scenarios remain program-owned only when interaction is part of the evidence.

## Test boundary

Architecture tests inspect ownership and include rules. Installed-consumer tests prove that exported public headers work without private paths. White-box tests may use private helpers through a deliberate test target. Integration tests may join modules without changing their ownership. GPU and stress tests remain separately labeled operational evidence.

A white-box include never promotes the included header to public API. Conversely, a public installed-consumer failure cannot be dismissed because an in-tree test happened to see extra include paths.

Test support has its own owner. Cross-cutting deterministic helpers live under `Tests/Support`; module-specific tests live under the owning module’s `Tests/` directory and register through the owner-local module test helper. A `Tests/.gitkeep` is removed once real coverage exists, and uppercase empty tool test shadows are not retained beside canonical lowercase `tests/`. Production targets do not link GoogleTest or test fixtures. GPU native helpers can include private Diligent contracts only through clearly private/GPU targets and never through `SagaGraphics` install/export.

Tests that scan repository paths receive the repository root explicitly and normalize separators. They do not depend on a developer working directory or historical root. Pattern checks distinguish code/include/type use from comments so documentation can name forbidden vendors without failing an API leak test.

## Retired paths

Do not reintroduce root `Runtime/`, `Server/`, `Backends/`, `Apps/`, `Shared/`, `Collaboration/`, `Content/`, `core/`, `Tools/scripts/`, or `docs/` as compatibility owners. When a real consumer requires a stable include, preserve or design the public contract under its current owner; do not build a forwarding shadow tree for mechanical compatibility.

Historical targets and names can remain in Git history. Current CMake, tests, scripts, and Wiki text should use post-cutover paths and current owners.

## Review checklist

Before accepting an ownership change:

- identify the semantic owner;
- list public consumers and private implementation users separately;
- inspect includes and type names, not comments alone;
- inspect target source membership and dependency visibility;
- confirm install/export consequences;
- update focused architecture and installed-consumer evidence;
- avoid compatibility aliases unless a supported public consumer requires them;
- keep aggregate targets as object composition and keep module OBJECT targets out of install/export.
