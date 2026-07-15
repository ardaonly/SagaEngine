---
title: Editor
status: current
owner: SagaWiki
generated_html: pages/editor.html
---

# Editor

The editor source is divided by responsibility. EditorCore owns toolkit-neutral editor state and services. EditorFramework owns toolkit-neutral commands, layouts, panels, personas, and workflow contracts. EditorQt owns the Qt application, windows, settings, panels, and toolkit adapters. EditorAuthoring owns scene, prefab, world, asset, pipeline, gizmo, and inspection workflows.

EditorScripting owns managed authoring integration, while VisualBlocksEditor owns block projection and block-editing presentation over SagaScript artifacts. EditorCollaboration owns local collaboration contracts and services; it does not establish a hosted collaboration product. EditorExperimental contains explicitly non-stable experiments. Program entry points under `Engine/Source/Programs` remain thin and keep program-specific implementation private.

## Authoring boundary

The editor may request, preview, display, and review source-changing operations through SagaScript-owned artifacts. It does not become the C# compiler, canonical behavior store, runtime authority, package authority, or an independent direct source writer.

## Personal view and shared state

The source-controlled project model is shared state; workspace layout, selection, viewport, cursor, and other personal editor-view state remain local to each participant. Changes to shared project meaning are represented as semantic transactions, while personal view changes are not project transactions. Local locks, presence, review, and conflict metadata do not establish a hosted collaboration service or claims of authentication, authorization, transport security, or enterprise security.

## Qt boundary

Qt headers and types must not leak through generic `SagaEditor/` public contracts. Public Qt-facing contracts, when unavoidable, use `SagaEditorQt/`. GraphCanvas is currently a private EditorQt implementation detail rather than a public ABI.
