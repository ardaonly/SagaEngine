---
title: Experimental editor extensions
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Experimental editor extensions

EditorExperimental currently contains an in-process extension experiment: extension points, a static instance registry, a host, lifecycle interfaces, command and panel interfaces, bridges, context, and serialization. This is useful implementation groundwork. Its location and current evidence deliberately classify it as experimental rather than a stable plugin contract.

## Current surface

| Area | Checked-in types | Bounded meaning |
| --- | --- | --- |
| Description | `ExtensionPoint` | Named in-process integration locations. |
| Registration | `ExtensionRegistry` | Explicit registration of already-created extension instances, with deterministic order and replacement tests. |
| Lifecycle | `IEditorExtension`, `ExtensionHost`, `IExtensionContext` | Load/unload coordination against current editor services. |
| Commands | `IExtensionCommand`, `ExtensionCommandBridge` | An extension-shaped command adapted to the editor command surface. |
| Panels | `IExtensionPanel`, `ExtensionPanelBridge` | An extension-shaped panel adapted to the current shell. |
| State | `IExtensionSerializer` | An in-process serialization hook, not a long-term compatibility format. |

The host currently coordinates extension lifecycle and can expose editor shell/framework capabilities through a context. An interface being under `Public` means another current target can include it; it does not by itself guarantee third-party binary compatibility, distribution support, or permanent naming.

## Relationship to stable editor owners

EditorCore owns editor-neutral values and contracts. EditorFramework owns the shell, commands, panels, workspace, and orchestration. EditorQt owns Qt-facing implementation. EditorExperimental may adapt those surfaces for experiments, but it must not duplicate their source of truth.

An extension command should route through the existing command model. An extension panel should participate in the existing panel/workspace lifecycle. Extension state must not become a hidden project model or bypass semantic authoring transactions.

Qt types should remain in EditorQt or private Qt implementation. An experimental extension contract that requires a `QWidget` directly would make the experimental layer a second Qt boundary and would prevent non-Qt inspection/testing.

## Compatibility status

There is no declared stable plugin ABI, dynamic loader, discovery directory, or plugin package schema. The repository does not promise that an extension compiled for this revision will load in a later revision. Extension points, lifecycle order, serialization bytes, and available context operations may change while the surface remains under EditorExperimental.

Because there is no stable public plugin domain or plugin contract, historical plugin licensing material is not current SagaWiki policy. It remains private/history reference for a future decision if a real contract is introduced. Current licensing authority remains `LICENSE_POLICY.toml` and its registered domains.

## Loading and trust

Loading an in-process native extension is equivalent to loading code into the editor process unless a future isolation design proves otherwise. The current interfaces do not establish sandboxing, signature verification, permission isolation, dependency resolution, remote acquisition, automatic updates, or malicious-extension containment.

A registry entry is not approval to execute untrusted code. The former declaration-only manifest/loader surface was removed because returning no extension was not a loading contract. Any future public loading workflow must define provenance, compatible versions, failure isolation, allowed capabilities, distribution, and licensing before it can make a plugin-product claim.

## Serialization and project truth

`IExtensionSerializer` is a hook for current extension state. Its byte vector has no implied durable schema unless the owning extension declares and validates one. Personal panel/view state should remain personal. Project-owned authored state must use the project source contracts and semantic transactions rather than an opaque extension blob.

Unknown or stale extension data must not silently override C#, scenes, project manifests, or asset source. If an extension produces derived artifacts, it records provenance and freshness like other generators.

## Failure behavior

Extension load failure should identify the extension and failed stage, leave the editor in a coherent state, and unwind already registered commands/panels. Unload must detach callbacks before destroying extension state. A failed extension must not be reported as a successfully available editor capability.

These are required review properties, not a statement that complete hostile-extension isolation is currently implemented.

## What would justify promotion

Promotion out of EditorExperimental would require a deliberate contract review covering at least lifecycle/versioning, manifest schema, stable extension points, error handling, packaging/discovery, compatibility policy, security/trust, licensing domain, installed-consumer evidence, and migration behavior. Until then, documentation should describe the concrete experimental surface and avoid the words “plugin SDK” as a current product capability.

For the stable editor ownership boundary see [Editor shell and authoring contracts](editor-shell-and-authoring.md). For current non-claims see [Product and capability reference](product-and-capability-boundaries.md).
