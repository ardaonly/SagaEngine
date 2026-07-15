---
title: Module boundaries
status: current
owner: SagaWiki
generated_html: pages/module-boundaries.html
---

# Module boundaries

## Runtime

Core, Math, Diagnostics, ECS, Assets, Resources, Input, Audio, Render, RHI, UI, Scripting, Networking, Replication, Persistence, Simulation, World, ServerAuthority, and SagaRuntime are explicit source owners.

## Editor

EditorCore and EditorFramework cannot expose Qt. EditorQt owns Qt-backed public spelling under `SagaEditorQt/`. VisualBlocksEditor owns `SagaEditor/VisualBlocks/`; the retired VisualScripting include and namespace are not aliases.

## Public/private

`Public/` is an intentional consumer contract. `Private/` is implementation-only. Public runtime headers must not expose vendor-specific types. Installed headers are assembled from module public roots.

## Native graphics ownership

Diligent device, immediate-context, swapchain, and native-resource ownership stays in the private RHI/Render implementation. SagaDiligentRuntime is the single native lifecycle owner; public headers must remain vendor-neutral, and no second device/context/swapchain creation path may be introduced.

## Intentional include cutovers

- `SagaEngine/Networking`, `SagaEngine/Replication`, and `SagaEngine/ServerAuthority`
- `SagaEngine/Persistence`
- `SagaEngine/UI/Backends`
- `SagaEditorQt`
- `SagaEditor/VisualBlocks`
