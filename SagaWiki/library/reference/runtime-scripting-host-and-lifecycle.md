---
title: Runtime scripting host and lifecycle
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Runtime scripting host and lifecycle

The Runtime Scripting module owns the controlled path from validated script artifacts to managed instances and lifecycle callbacks. SagaScript owns source analysis and compilation workflows; VisualBlocksEditor owns an authoring projection; the runtime host consumes produced artifacts. C# source remains behavior truth throughout that chain.

## Current layers

The checked-in public surface has three distinguishable layers:

| Layer | Responsibility |
| --- | --- |
| Shared descriptors | Script identity, artifact references, binding descriptions, requested capabilities, authority, generated-code origin, and diagnostic payloads. |
| Low-level host contracts | Host and world interfaces, handles, lifecycle requests/results, package validation, and deterministic diagnostic values. |
| Runtime services | Manifest loading, binding orchestration, C# host integration, lifecycle ordering, and the `WorldState` adapter. |

Compatibility forwarding headers currently expose some types through more than one include path. Their presence records transitional source compatibility inside the current tree; it is not permission to create another scripting implementation or to treat every forwarding header as an independently versioned API.

## Validation precedes host loading

A runtime package is not accepted because an assembly file exists. Before reaching the managed host, the lifecycle path validates the manifest and its relationship to the selected package. Focused tests cover missing manifests and assemblies, paths escaping the package boundary, destination mismatch, unsupported target frameworks, absent or malformed SHA-256 values, changed assembly bytes, missing authority, authority/destination mismatch, and malformed capability metadata.

The important ordering rule is that rejected metadata does not reach the host. This prevents a permissive host fallback from turning a package error into execution. Validation diagnostics must be deterministic enough for tests and tools to classify, while retaining bounded context for a human reader.

Artifact hashes establish freshness and identity for the checked bytes. They do not establish trust in the source author, sandbox arbitrary managed code, or replace capability and authority checks.

## Binding and instance creation

Binding manifests connect generated script identifiers to classes, artifacts, entity policy, authority, and capabilities. `ScriptBindingManifestLoader` parses that contract; `ScriptBindingRuntime` plans instance creation and applies it through the selected host/world boundary.

Entity policy is explicit. An existing-self binding must resolve the declared entity before host load. A create-self binding creates the named entity through the world interface before instance construction. Duplicate generated binding identifiers, missing script identifiers, missing entity keys, create-name conflicts, and capability denial fail rather than producing a partially believable binding set.

Binding startup is transactional at the bounded runtime level. Focused tests verify that a partial instance-creation failure destroys already created instances and that normal shutdown runs in reverse creation order.

## Lifecycle order

The current lifecycle vocabulary includes creation, start, update, and destruction. The service orders callbacks deterministically for its registered instances and passes the update delta explicitly. Invalid handles and managed exceptions become diagnostics rather than unchecked native behavior.

`CSharpScriptHost` provides the current managed hosting implementation. Tests demonstrate a minimal managed script lifecycle, script context access, bounded logging, world entity creation and position mutation, self-entity access, UI named-action invocation, missing class/config/assembly failures, invalid signatures, false-return handling, and managed exceptions. Each is focused evidence for that path; it is not a general compatibility promise for arbitrary assemblies or every .NET feature.

## Capabilities and authority

Capabilities make native access opt-in. A script package declares requested capabilities, the hosting workflow supplies grants, and validation rejects missing, unexpected, duplicate, malformed, authority-incompatible, or destination-incompatible entries. Empty grants remain distinct from a missing or malformed capability manifest.

Authority describes where behavior is permitted to run, such as client, server, or shared destinations where the current contract allows them. Authority metadata is not network authentication or process isolation. It constrains host admission and API access; it does not make untrusted managed code safe to execute.

The world interface is deliberately narrower than direct access to internal state. `WorldStateScriptWorld` adapts the Simulation state contract into script-facing operations. Adding a script capability should therefore add a narrow interface operation, a validation rule, deterministic diagnostics, and focused tests rather than exposing a subsystem singleton.

## UI and other integrations

Named UI actions can route through `CSharpUiNamedActionHandler` to a managed method while preserving the script context. UI owns event collection and action dispatch; Scripting owns host invocation. Neither module should parse the other's implementation details.

The same rule applies to future audio, networking, persistence, or editor integrations. A public script API is a capability contract, not a direct include bridge to the native implementation. Low-level and unsafe access must remain explicit and reviewable.

## Relationship to authoring

Visual Blocks can project supported C# and propose source patches. It does not submit a graph to this runtime. SagaScript can compile/analyze source and emit binding/artifact metadata. The runtime does not silently rewrite source, regenerate stale artifacts, or decide that a graph is newer than C#.

The durable flow is:

```text
C# source
  -> SagaScript analysis/compile
  -> binding and artifact manifests plus assembly
  -> package/path/hash/capability/authority validation
  -> binding plan and managed instances
  -> ordered lifecycle through narrow native ports
```

## Non-claims

The current host and tests do not establish a security sandbox, stable binary compatibility for third-party scripts, arbitrary NuGet/package support, production hot reload, remote code distribution, or a second graph runtime. A test fixture loading a managed assembly is evidence for that fixture and host environment only.

For authoring and source patch rules, see [C# and Visual Blocks contracts](csharp-and-visual-blocks-contracts.md). For project and artifact authority, see [Source, artifact, and project contracts](source-artifact-and-project-contracts.md).

