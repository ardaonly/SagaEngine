---
title: Editor contract
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Editor contract

The editor is a composition of owned modules rather than one undifferentiated application layer. EditorCore owns editor-neutral concepts, EditorFramework owns orchestration and extensibility, EditorQt owns Qt-facing integration, EditorAuthoring owns project authoring workflows, and focused modules own scripting, Visual Blocks, collaboration, and experimental surfaces.

## Authoring boundary

Editor operations should express semantic changes to project-owned source: create an entity, change a property, apply a script patch, or update an asset reference. UI widgets are views and command initiators. They should not become a hidden second store for project behavior.

The personal editor view includes window layout, panel visibility, selection, filters, and other per-user presentation state. Shared project state includes authored entities, assets, scripts, and semantic transactions. Saving personal view state must not mutate the shared project model accidentally.

## Qt boundary

Qt types belong to EditorQt or private Qt implementation. Runtime public headers and platform-neutral editor contracts should not require `QObject`, `QWidget`, `QString`, or other Qt ABI. This keeps non-Qt tools and tests able to consume the editor-neutral contracts.

## Customization

Durable customization is contract-based: registered commands, panels, inspectors, authoring actions, and extension descriptors. A concrete widget tree or application singleton is not a stable extension API merely because it is visible in a public include path.

EditorExperimental currently contains extension manifests, registry/loading, host lifecycle, context, command/panel bridges, and a serialization hook. Those public headers are an experimental in-process surface. They do not establish a stable plugin ABI, package schema, compatibility window, security sandbox, distribution channel, or plugin licensing domain.

## Minimum shell workflow

The current shell and authoring surfaces support bounded project inspection, panel/workspace configuration, selection, save/undo/redo routing, script evidence views, and scenario-driven checks. A report or scenario action proves only that routed workflow; it must not be described as a completed interactive dashboard. EditorLab remains a development/scenario program, not a second editor contract owner.

## Product limit

The repository contains editor modules, programs, and workflow evidence, but SagaWiki does not claim a finished editor product, complete asset pipeline, polished onboarding, or stable third-party plugin API.

See [Editor shell and authoring contracts](../reference/editor-shell-and-authoring.md) for the complete module, shell, Qt, panel, transaction, inspector, scene/asset, customization, and workflow boundary.

The current extension-shaped API and its explicit limits are documented in [Experimental editor extensions](../reference/experimental-editor-extensions.md).
