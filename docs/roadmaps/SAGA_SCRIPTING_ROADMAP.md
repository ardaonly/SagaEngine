# SagaScript Roadmap

> Last updated: 2026-05-20
> Status: Proposed scripting architecture roadmap
> Target: A production-grade SagaScript ecosystem that uses C# as the first text scripting language while sharing one authority-aware model across visual blocks, generated code, freeform scripts, server logic, runtime logic, and approved native extension points.
> Scope: C# scripting, visual block integration, generated C# preview, block-callable bindings, script validation and compile, binding manifests, script artifacts, capability metadata, permission-aware authoring, runtime/server script consumption, diagnostics, Forge orchestration, Prism analysis, editor UX, collaboration resources, package gates, and native/engine extension boundaries.

---

## 0. Roadmap Convention

* `[x]` - Shipped. The note after the item names concrete files, commands, tests, manifests, artifacts, or integration points that represent completed work.
* `[ ]` - Open. The item describes the finished production state, not temporary scaffolding.
* Shipped scripting work must name artifact formats, binding manifests, diagnostics, tests, and consuming modules where practical.
* Open scripting work must describe observable authoring, build, package, runtime, or server behavior.
* SagaScript must not become editor-private truth.
* SagaScript must not bypass server authority.
* SagaScript must not make runtime/server compile source files during gameplay.
* SagaScript must not put scripting host implementation in `SagaShared`.
* SagaScript must not make Forge the C# compiler.
* SagaScript must not make Prism generate or compile artifacts.
* SagaScript must not make SDE depend on Saga modules, scripting host internals, or C# compiler internals.
* SagaScript capability checks are not UI preferences.

SagaScript is the product-facing scripting ecosystem.

C# is the first text language for that ecosystem.

The goal is one coherent authoring and execution chain, not a split where blocks are safe, text scripts are unsafe, and native extensions are a back door.

---

## 1. Document Purpose

This document defines the roadmap for SagaScript.

SagaScript connects:

* visual blocks,
* graph-generated C# preview,
* annotated C# block-callable APIs,
* freeform C# scripts,
* server-side scripts,
* client/runtime scripts,
* editor/build/tool scripts where explicitly allowed,
* capability-gated native and engine extension points,
* script artifacts,
* binding manifests,
* authority metadata,
* diagnostics,
* collaboration and package gates.

Correct model:

```txt
Blocks / Graphs / C# Source
      |
      v
SagaScript validation + binding metadata
      |
      v
Script artifacts + binding manifests + capability manifests
      |
      v
Forge package gates / Prism analysis
      |
      v
Runtime and Server consume packaged artifacts
```

Incorrect model:

```txt
Runtime watches source folders, compiles random C#, and lets scripts call engine internals.
```

That is not scripting flexibility.

That is an authority and security failure with syntax highlighting.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md` | Gameplay graph, blocks, generated C# preview, block bindings |
| `AUTHORING_AUTHORITY_MODEL.md` | Authority, execution domains, side effects, security boundaries |
| `AUTHORING_PERSONAS.md` | C# gameplay programmer and profile-driven surface visibility |
| `EDITOR_ROADMAP.md` | Script editor UX, binding inspector, generated preview, diagnostics display |
| `SHARED_ROADMAP.md` | Neutral scripting, artifact, authority, diagnostic, and package contracts |
| `ENGINE_ROADMAP.md` | Runtime/server package consumption and script binding consumption |
| `ClientReplicationFormalSpec.md` | Client prediction, server authority, and replication correctness |
| `FORGE_ROADMAP.md` | Script compile orchestration, package staging, publish gates |
| `PRISM_ROADMAP.md` | C# source analysis, binding manifest freshness, boundary reports |
| `COLLABORATION_ROADMAP.md` | Script resource locks, permissions, conflicts, publish blockers |
| `DIAGNOSTICS_ROADMAP.md` | Structured diagnostics and report visibility |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md` | Validate, compile, cook, package, publish readiness flow |
| `DependencyGraph.md` | Ownership and dependency boundaries |

---

## 3. Product Definition

* [ ] Define SagaScript as one scripting ecosystem.

  Done means:

  * visual blocks, generated code, annotated C#, and freeform C# share authority, side-effect, capability, diagnostic, and package rules,
  * users can write gameplay logic as blocks or text without bypassing validation,
  * advanced users can access approved extension points through explicit capabilities,
  * server authority remains mandatory for authoritative gameplay mutation.

* [ ] Use C# as the first SagaScript text language.

  Done means C# scripts can:

  * define gameplay behavior,
  * expose block-callable APIs through explicit metadata,
  * participate in server-side execution where approved,
  * participate in runtime/client execution where approved,
  * produce diagnostics and binding metadata for editor, Forge, Prism, runtime, and server consumers.

* [ ] Keep SagaScript separate from engine core mutation.

  Done means scripts can call approved public scripting APIs and extension surfaces, but cannot directly mutate engine core internals unless a specific capability and host boundary allows it.

---

## 4. Ownership Model

### 4.1 Saga Product Ownership

* [ ] `Saga` owns product-level scripting workflows.

  Done means Saga can route:

  * authoring profile selection,
  * project scripting settings,
  * validate/build/package/publish entry points,
  * product-level scripting diagnostics,
  * scripting publish blockers.

* [ ] `Saga` does not own scripting compiler internals.

  Done means Saga consumes service/tool results instead of compiling or hosting scripts itself.

---

### 4.2 SagaEditor Ownership

* [ ] `SagaEditor` owns scripting authoring UX.

  Done means the editor owns:

  * script editor integration,
  * generated C# preview display,
  * block binding inspector UI,
  * script diagnostics display,
  * script artifact status display,
  * capability and authority metadata display,
  * hot reload controls where allowed.

* [ ] `SagaEditor` does not own script truth.

  Done means the editor does not own:

  * C# compiler implementation,
  * binding manifest generation,
  * runtime/server script host internals,
  * server authority policy,
  * package staging truth.

---

### 4.3 SagaShared Ownership

* [ ] `SagaShared` owns neutral scripting contracts only.

  Valid shared contracts include:

  * `ScriptId`,
  * `ScriptArtifactRef`,
  * `ScriptBindingDescriptor`,
  * `ScriptCapabilityDescriptor`,
  * `ScriptAuthorityDescriptor`,
  * `ScriptDiagnosticPayload`,
  * `GeneratedCodeOriginDescriptor`,
  * script/package manifest references.

* [ ] `SagaShared` does not own scripting implementation.

  Forbidden in `SagaShared`:

  * C# compiler hosts,
  * runtime script host implementation,
  * server script execution implementation,
  * editor script panels,
  * hot reload implementation,
  * native extension loaders,
  * Roslyn or CoreCLR implementation bindings.

---

### 4.4 Scripting Toolchain Ownership

* [ ] Scripting Toolchain owns script validation, compile, binding extraction, artifact emission, and script diagnostics.

  Done means the scripting toolchain can:

  * validate C# source metadata,
  * validate block-callable methods,
  * validate authority metadata,
  * validate side-effect metadata,
  * validate requested capabilities,
  * emit script artifacts,
  * emit binding manifests,
  * emit generated-code origin metadata,
  * emit structured diagnostics.

* [ ] Scripting Toolchain does not own runtime/server script execution.

  Done means the toolchain produces artifacts and reports; runtime/server bind and execute approved artifacts through their own host boundaries.

---

### 4.5 Runtime Ownership

* [ ] Runtime consumes client-authorized script artifacts only.

  Done means runtime can load scripts for:

  * client-only UI logic,
  * visual-only behavior,
  * prediction-safe behavior,
  * local runtime presentation logic,
  * approved runtime extension points.

* [ ] Runtime rejects invalid script artifacts.

  Done means runtime does not execute:

  * server-only scripts,
  * scripts with missing capability manifests,
  * scripts with incompatible artifact versions,
  * scripts requesting forbidden capabilities,
  * source files not represented by packaged artifacts.

---

### 4.6 Server Ownership

* [ ] Server owns authoritative SagaScript execution.

  Done means server can execute approved script artifacts for:

  * authoritative gameplay logic,
  * request validation,
  * combat, quest, inventory, ability, economy, spawn, and persistence logic,
  * authoritative replication source mutation.

* [ ] Server treats client script requests as untrusted intent.

  Done means client-originated script calls route through validation before authoritative effects occur.

---

### 4.7 Forge Ownership

* [ ] Forge orchestrates SagaScript validation, compile, and package gates.

  Done means Forge can:

  * invoke the scripting toolchain,
  * collect script diagnostics,
  * fail builds based on severity and profile,
  * stage script artifacts into allowed package destinations,
  * reject stale or missing binding manifests,
  * write build/package/publish reports.

* [ ] Forge does not compile C# directly as an owned implementation.

  Done means Forge uses a scripting toolchain adapter, command boundary, or approved service boundary.

---

### 4.8 Prism Ownership

* [ ] Prism analyzes SagaScript relationships.

  Done means Prism can report:

  * stale binding manifests,
  * stale generated C#,
  * source-to-artifact mismatches,
  * capability metadata mismatch,
  * forbidden dependency direction,
  * server-only scripts staged into client packages,
  * generated-code origin issues.

* [ ] Prism does not generate or compile scripts.

  Done means Prism reads source, generated outputs, manifests, and reports, then emits analysis.

---

## 5. SagaScript Source Model

* [ ] Support visual blocks as a first-class scripting source.

  Done means block/graph users can produce validated script-facing behavior without hand-writing C#.

* [ ] Support generated C# preview from graph IR.

  Done means users can inspect generated C# as read-only preview unless they explicitly detach it into editable script source.

* [ ] Support annotated C# to block definitions.

  Done means C# methods/classes can expose block-callable definitions only through explicit metadata.

  Example concept:

```csharp
[BlockCallable]
[BlockCategory("Inventory")]
[BlockName("Give Item")]
[ServerOnly]
[WritesPersistentState]
[RequiresCapability("Gameplay.Inventory.Write")]
public static void GiveItem(PlayerRef player, ItemId item, int count)
{
}
```

* [ ] Support freeform C# as script artifacts.

  Done means freeform C# can ship as validated script artifacts or custom script nodes without pretending it can always round-trip into clean blocks.

---

## 6. Authority and Side Effects

* [ ] Require authority metadata for side-effecting scripts.

  Done means C# APIs that expose gameplay behavior must declare authority metadata compatible with the authoring authority model.

* [ ] Require side-effect metadata.

  Done means script bindings declare whether they:

  * are pure,
  * read or write local state,
  * read or write authoritative state,
  * read or write replicated state,
  * send network events,
  * read or write persistence,
  * allocate runtime resources,
  * schedule jobs or timers,
  * emit diagnostics,
  * mutate editor/build/tool state.

* [ ] Reject hidden side effects.

  Done means script bindings that mutate authoritative, replicated, persistent, editor, build, or tool state without declaring it fail validation.

---

## 7. Capability Model

Capability describes what a script is allowed to access.

Authoring profile describes what UI depth a user sees.

Collaboration permission describes what project actions a user may perform.

These are separate systems.

Required initial capability domains:

```txt
SharedPure
ClientPresentation
ClientRequest
ServerValidation
TrustedServerOnly
EditorTooling
BuildTooling
NativeExtension
EngineInternal
```

* [ ] Add capability metadata to script bindings and artifacts.

  Done means every script artifact can describe requested and granted capabilities.

* [ ] Gate native and engine extension access.

  Done means native/engine extension access requires explicit capability metadata and cannot be reached by ordinary gameplay scripts by default.

* [ ] Reject capability escalation.

  Done means a script cannot gain stronger capabilities by being converted from blocks to C#, by being detached from generated code, or by being moved between package destinations.

* [ ] Keep `EngineInternal` restricted.

  Done means `EngineInternal` is reserved for engine-owned implementation and approved extension surfaces, not general project scripting.

---

## 8. Artifacts and Manifests

* [ ] Define script artifact outputs.

  Done means script compile produces runtime/server-consumable artifacts with stable identity, version, source hash, compiler/toolchain version, authority metadata, capability metadata, binding references, and diagnostics references.

* [ ] Define binding manifests.

  Done means binding manifests describe block-callable C# APIs, parameters, return types, authority, side effects, capabilities, source locations, generated-code origins, and artifact refs.

* [ ] Define capability manifests.

  Done means capability manifests describe requested and granted capabilities per script artifact and package destination.

* [ ] Keep exact manifest schema versioned and documented before implementation.

  Done means any concrete manifest format is approved before runtime/server/package consumers depend on it.

---

## 9. Editor UX

* [ ] Add script editor integration.

  Done means users can create, open, edit, validate, and inspect C# scripts through editor UX while validation and compile route through approved tool/service boundaries.

* [ ] Add block binding inspector.

  Done means users can inspect `[BlockCallable]` bindings, parameter mapping, authority metadata, side effects, capability requirements, and binding diagnostics.

* [ ] Add generated C# preview.

  Done means graph users can inspect generated C# without treating generated code as source truth.

* [ ] Add conversion UX.

  Required operations:

```txt
View generated C#
Convert graph to C# script
Expose C# method as block
Create custom script node
Detach generated code as editable script
```

* [ ] Warn about non-reversible conversions.

  Done means users are told when freeform C# may not reconstruct into visual blocks.

---

## 10. Runtime and Server Consumption

* [ ] Runtime/server load packaged script artifacts and manifests.

  Done means runtime/server consume package manifests, script artifacts, binding manifests, authority manifests, and capability manifests without reading source folders.

* [ ] Validate artifacts before execution.

  Done means runtime/server reject missing, stale, incompatible, unauthorized, wrong-destination, or wrong-authority script artifacts.

* [ ] Keep source compilation out of runtime gameplay.

  Done means runtime/server gameplay loops never parse or compile project C# source as normal execution behavior.

* [ ] Define hot reload policy before implementation.

  Done means development hot reload, editor preview reload, server reload, and shipping behavior have explicit rules before any host implementation is treated as production behavior.

---

## 11. Collaboration and Permissions

* [ ] Treat scripts as collaboration resources.

  Done means scripts expose resource descriptors for:

  * C# script source,
  * generated C# output,
  * binding manifest,
  * script artifact,
  * capability manifest,
  * generated-code origin metadata.

* [ ] Respect collaboration permissions.

  Done means `ScriptEdit`, `GraphEdit`, `ServerAuthorityEdit`, `BuildRun`, `PackageStage`, and `PublishRelease` permissions remain separate from authoring profiles and script capabilities.

* [ ] Add script conflicts and publish blockers.

  Done means unresolved script conflicts, stale binding manifests, or capability mismatches can block strict build/package/publish profiles.

---

## 12. Diagnostics

* [ ] Define SagaScript diagnostic families.

  Required diagnostic families:

```txt
Script.Syntax
Script.Type
Script.Binding
Script.Authority
Script.Capability
Script.SideEffect
Script.Artifact
Script.Package
Script.GeneratedCode
Script.Collaboration
Script.Host
Script.Security
```

* [ ] Emit structured diagnostics.

  Done means diagnostics include code, severity, script id, source range, binding id where available, artifact ref where available, authority context, capability context, package destination, and suggested correction where safe.

* [ ] Surface diagnostics across systems.

  Done means editor, Forge, Prism, CI, runtime, server, and Saga product workflows can consume script diagnostics through reports or shared payloads.

---

## 13. Build and Publish Gates

* [ ] Add script validation build gate.

  Done means Forge can fail build when script diagnostics exceed profile thresholds.

* [ ] Add script artifact staging gate.

  Done means package staging rejects:

  * server-only scripts in executable client packages,
  * client-only scripts used as server truth,
  * editor/build/tool scripts in runtime packages,
  * artifacts without authority metadata,
  * artifacts without capability metadata,
  * stale binding manifests,
  * stale generated-code origins.

* [ ] Add publish gate.

  Done means publish fails when script artifacts, binding manifests, authority metadata, capability metadata, or package destinations disagree.

---

## 14. Testing Strategy

* [ ] Add scripting contract tests.

  Required coverage:

  * script ids,
  * binding descriptors,
  * artifact refs,
  * authority descriptors,
  * capability descriptors,
  * diagnostic payloads.

* [ ] Add binding validation tests.

  Required coverage:

  * `[BlockCallable]` extraction,
  * missing authority metadata,
  * missing capability metadata,
  * unsupported parameter type,
  * hidden side effect rejection,
  * generated-code origin tracking.

* [ ] Add package gate tests.

  Required coverage:

  * server-only script rejected from client executable package,
  * client-only script rejected as server authority,
  * stale binding manifest rejected,
  * missing capability manifest rejected,
  * invalid artifact version rejected.

* [ ] Add runtime/server consumption tests.

  Required coverage:

  * runtime loads allowed client artifact,
  * server loads allowed authoritative artifact,
  * runtime rejects server-only artifact,
  * server rejects missing capability metadata,
  * source files are not used as runtime truth.

---

## 15. MVP Target

* [ ] Build the first SagaScript vertical slice.

  Required scenario:

```txt
Quest reward script exposed as a block and executed as server authority.
```

  Required authoring behavior:

```txt
C# method declares BlockCallable, ServerOnly, persistent write, replication effect, and inventory capability.
Editor displays the binding.
Graph can call the block in a ServerOnly quest reward graph.
ClientOnly graph placement is rejected.
Forge validates and stages the server artifact.
Server consumes the artifact or binding descriptor.
Prism detects stale binding metadata.
```

  Required implementation evidence:

  * C# script source exists,
  * binding manifest is emitted,
  * capability metadata is emitted,
  * authority diagnostics are emitted,
  * Forge gate catches invalid placement,
  * Prism reports stale binding state where applicable,
  * server-side consumer rejects invalid artifact metadata.

---

## 16. Non-Goals

SagaScript must not become:

* a replacement for C++ engine development,
* a way for arbitrary users to mutate engine core,
* a runtime source compiler,
* a server authority bypass,
* an editor-private script host,
* a Forge-owned compiler,
* a Prism-owned code generator,
* a SagaShared implementation layer,
* a promise that arbitrary C# can always become visual blocks,
* a shortcut around collaboration permissions.

---

## 17. Risk Register

### 17.1 Risk: C# Bypasses Graph Safety

Mitigation:

* require metadata for block-callable APIs,
* validate authority and capabilities,
* keep package gates strict,
* reject missing metadata for side-effecting behavior.

---

### 17.2 Risk: Runtime Becomes The Compiler

Mitigation:

* compile through the scripting toolchain,
* package artifacts before runtime/server consumption,
* keep runtime/server source parsing out of gameplay execution.

---

### 17.3 Risk: Capability Model Becomes UI Visibility

Mitigation:

* keep authoring profiles, collaboration permissions, and script capabilities separate,
* enforce capabilities in toolchain/package/runtime/server boundaries,
* do not rely on hidden UI controls as security.

---

### 17.4 Risk: Native Extensions Become A Back Door

Mitigation:

* require explicit capabilities,
* keep `EngineInternal` restricted,
* document host boundaries,
* emit diagnostics for forbidden capability requests.

---

## 18. Suggested File Targets

Expected shared contracts:

```txt
Shared/include/SagaShared/Scripting/ScriptId.hpp
Shared/include/SagaShared/Scripting/ScriptArtifactRef.hpp
Shared/include/SagaShared/Scripting/ScriptBindingDescriptor.hpp
Shared/include/SagaShared/Scripting/ScriptCapabilityDescriptor.hpp
Shared/include/SagaShared/Scripting/ScriptAuthorityDescriptor.hpp
Shared/include/SagaShared/Scripting/GeneratedCodeOriginDescriptor.hpp
Shared/include/SagaShared/Scripting/ScriptDiagnosticPayload.hpp
```

Expected editor surfaces:

```txt
Editor/include/SagaEditor/Scripting/ScriptEditorPanel.h
Editor/include/SagaEditor/Scripting/BlockBindingInspectorPanel.h
Editor/include/SagaEditor/Scripting/GeneratedCodePreviewPanel.h
Editor/include/SagaEditor/Scripting/ScriptDiagnosticAdapter.h
```

Expected toolchain surfaces:

```txt
Scripting toolchain validate command
Scripting toolchain compile command
Scripting toolchain binding manifest output
Scripting toolchain capability manifest output
Scripting toolchain diagnostics report
```

Exact scripting toolchain package name, C# compiler host, runtime host technology, and manifest schema format require approval before implementation.

---

## 19. Decision Summary

Preserve these decisions:

```txt
SagaScript is the product-facing scripting ecosystem.
C# is the first text language.
Blocks and C# share authority, side-effect, capability, diagnostic, and package rules.
Graph to generated C# is supported.
Annotated C# to block definition is supported.
Arbitrary C# to lossless blocks is not guaranteed.
Scripting Toolchain owns validation, compile, binding metadata, artifacts, and diagnostics.
Runtime/server consume packaged artifacts and manifests.
Forge orchestrates script toolchain steps and package gates.
Prism analyzes script source, generated output, manifests, artifacts, and boundaries.
SagaEditor owns authoring UX, not compiler or host internals.
SagaShared owns neutral contracts only.
Server authority is mandatory for authoritative gameplay mutation.
Native and engine extension access is capability-gated.
Authoring profiles, collaboration permissions, and script capabilities are separate.
```
