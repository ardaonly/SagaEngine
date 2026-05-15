# SagaEditor — Authoring Module Roadmap

> Last updated: 2026-05-14  
> Status: Active roadmap  
> Target: A production-grade authoring environment mounted inside the Saga product executable.  
> Scope: Editor shell, docking, panels, commands, viewport tools, asset authoring, inspector workflows, diagnostics, and editor-side collaboration UX.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files that represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item describes the finished state rather than interim scaffolding.
- Shipped items must name the files, modules, or integration points that represent the completed work.
- Open items must describe the finished production state, not temporary scaffolding.
- All editor headers live under `Editor/include/SagaEditor/...`.
- Matching translation units live under `Editor/src/SagaEditor/...`.
- The editor is a Qt-backed application surface, but SagaEditor public headers must stay Qt-free.
- Qt usage must remain hidden behind the `IUIApplication`, `IUIFactory`, `IUIMainWindow`, and `IUIWidget` interfaces under `Editor/include/SagaEditor/UI/`.

---

## 1. Document Purpose

This document is the source of truth for SagaEditor authoring systems.

It covers:

- editor shell,
- docking,
- panels,
- inspector workflows,
- hierarchy workflows,
- content browser workflows,
- viewport tools,
- command system,
- visual scripting UI,
- asset authoring surfaces,
- editor diagnostics,
- editor-side collaboration UI.

SagaEditor is not the primary product executable.

The production product direction is:

```txt
Saga = primary user-facing executable
Editor = authoring module/mode mounted by Saga
Apps/Editor = optional development compatibility launcher
```

This distinction is load-bearing.

The editor owns authoring UX.

Saga owns product lifecycle.

Anything else is how a tool window slowly becomes a fake operating system. Humans keep doing this, apparently for sport.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `SAGA_PRODUCT_ROADMAP.md` | Saga product shell, primary executable behavior, project lifecycle, mode orchestration |
| `ENGINE_ROADMAP.md` | Runtime, networking, replication, authoritative simulation, server systems |
| `SHARED_ROADMAP.md` | Neutral editor/runtime shared contracts |
| `COLLABORATION_ROADMAP.md` | Product collaboration, sessions, presence, permissions, conflicts |
| `DependencyGraph.md` | Dependency ownership and compile-time architecture boundaries |
| `TOOLS_ROADMAP.md` | Tool ecosystem ownership and boundaries |

---

## 3. Product Ownership Rule

- [x] Define SagaEditor as an authoring module instead of the primary product executable.

  Represented by:

  ```txt
  EDITOR_ROADMAP.md
  DependencyGraph.md
  COLLABORATION_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  Saga owns product lifecycle.
  SagaEditor owns authoring UX.
  ```

- [ ] Mount editor mode inside the Saga-owned product process.

  Done means:

  - `Saga` owns `QApplication`,
  - `Saga` owns the primary main window,
  - editor mode mounts into a Saga-owned surface,
  - editor mode does not create a second `QApplication`,
  - closing a project returns to the Saga product shell,
  - `Apps/Editor` remains optional and development-only.

- [ ] Remove production assumptions that treat `Apps/Editor` as the main user workflow.

  Done means:

  - user-facing project creation starts in `Saga`,
  - user-facing project opening starts in `Saga`,
  - mode switching is owned by `Saga`,
  - editor-only launcher paths are marked as development compatibility.

---

## 4. Editor Ownership

- [x] Define editor ownership boundaries.

  Represented by:

  ```txt
  EDITOR_ROADMAP.md
  DependencyGraph.md
  ```

  Preserved decision:

  ```txt
  Editor owns authoring tools.
  Editor does not own product shell, global session truth, or runtime/server authority.
  ```

- [ ] Keep the editor responsible for authoring UI only.

  Done means the editor owns:

  - panels,
  - docking,
  - inspector,
  - hierarchy,
  - viewport tools,
  - content browser,
  - editor commands,
  - editor preferences,
  - editor diagnostics panels,
  - collaboration display panels,
  - conflict resolution UI,
  - editor-local service adapters.

- [ ] Prevent editor code from owning product-level workflows.

  Done means the editor does not own:

  - Saga product shell,
  - project dashboard,
  - project creation as a product workflow,
  - project opening as a product workflow,
  - recent project registry truth,
  - session lifecycle truth,
  - final collaboration contracts,
  - runtime authority,
  - server authority,
  - process-level product orchestration.

---

## 5. Qt Boundary

- [x] Keep public SagaEditor UI headers Qt-free through UI abstraction interfaces.

  Represented by:

  ```txt
  Editor/include/SagaEditor/UI/IUIApplication.hpp
  Editor/include/SagaEditor/UI/IUIFactory.hpp
  Editor/include/SagaEditor/UI/IUIMainWindow.hpp
  Editor/include/SagaEditor/UI/IUIWidget.hpp
  ```

  Preserved decision:

  ```txt
  Qt is an implementation backend.
  SagaEditor public UI-facing contracts remain backend-neutral.
  ```

- [ ] Keep Qt implementation behind backend/private boundaries.

  Done means:

  - public editor headers do not expose Qt types,
  - Qt widgets stay in implementation files or approved Qt backend folders,
  - editor panels communicate through SagaEditor interfaces,
  - product integration does not require consumers to include Qt headers.

- [ ] Enforce Qt-free public headers through CI.

  Done means CI rejects public headers under:

  ```txt
  Editor/include/SagaEditor/**
  ```

  if they include Qt headers directly, except explicitly approved backend boundary headers.

- [ ] Keep `QApplication` ownership outside the editor module in production.

  Done means:

  - production `QApplication` is created by `Saga`,
  - editor mode receives an application/window abstraction,
  - editor mode does not own process lifetime,
  - `Apps/Editor` compatibility launcher is isolated from production architecture.

---

## 6. Editor Shell

- [ ] Provide a production editor shell mounted inside Saga.

  Done means:

  - editor shell can mount into Saga product window,
  - editor shell can unmount cleanly,
  - project close returns to Saga product shell,
  - layout state is restored per project/workspace,
  - editor shell has no hidden dependency on standalone launcher ownership.

Expected files:

```txt
Editor/include/SagaEditor/Shell/EditorShell.hpp
Editor/src/SagaEditor/Shell/EditorShell.cpp
Editor/include/SagaEditor/Shell/EditorMode.hpp
Editor/src/SagaEditor/Shell/EditorMode.cpp
```

- [ ] Keep development launcher separate from production editor shell.

  Done means:

  - `Apps/Editor` can launch editor for local development,
  - `Apps/Editor` does not define product architecture,
  - production users enter editor through `Saga`.

Expected files:

```txt
Apps/Editor/main.cpp
Apps/Saga/main.cpp
```

---

## 7. Docking and Layout System

- [ ] Provide dockable editor panels.

  Done means:

  - panels can be docked,
  - panels can be undocked,
  - panels can be hidden and restored,
  - layout persists per workspace,
  - layout reset is available,
  - docking system does not leak Qt types into public editor contracts.

Expected files:

```txt
Editor/include/SagaEditor/Docking/IDockManager.hpp
Editor/include/SagaEditor/Docking/DockPanelDescriptor.hpp
Editor/src/SagaEditor/Docking/DockManager.cpp
```

- [ ] Support named editor layouts.

  Done means:

  - default layout exists,
  - scripting layout can exist,
  - asset layout can exist,
  - debugging layout can exist,
  - custom layouts can be saved and restored.

- [ ] Add layout migration/versioning.

  Done means:

  - old layout files do not crash the editor,
  - missing panels are ignored safely,
  - renamed panels are migrated or skipped with diagnostics.

---

## 8. Panel Framework

- [ ] Define a standard panel lifecycle.

  Done means panels support:

  - creation,
  - mounting,
  - activation,
  - deactivation,
  - refresh,
  - serialization of local UI state,
  - clean destruction.

Expected files:

```txt
Editor/include/SagaEditor/Panels/IEditorPanel.hpp
Editor/include/SagaEditor/Panels/EditorPanelDescriptor.hpp
Editor/src/SagaEditor/Panels/EditorPanelRegistry.cpp
```

- [ ] Provide a panel registry.

  Done means:

  - panels register through descriptors,
  - duplicate panel ids are rejected,
  - panel construction is centralized,
  - panels can be discovered by shell/docking systems.

- [ ] Keep panel implementation behind editor module boundaries.

  Done means:

  - panels do not own product lifecycle,
  - panels do not directly own backend connections,
  - panels do not import server-private headers,
  - panels do not define shared collaboration contracts.

---

## 9. Core Editor Panels

### 9.1 Hierarchy Panel

- [ ] Add production-ready hierarchy panel.

  Done means:

  - scene entities are displayed,
  - parent/child relationships are visible,
  - selection syncs with inspector and viewport,
  - entity create/delete/rename actions exist,
  - invalid entity state is displayed clearly.

Expected files:

```txt
Editor/include/SagaEditor/Panels/HierarchyPanel.hpp
Editor/src/SagaEditor/Panels/HierarchyPanel.cpp
```

---

### 9.2 Inspector Panel

- [ ] Add production-ready inspector panel.

  Done means:

  - selected entity/resource properties are visible,
  - editable fields support validation,
  - component data can be edited,
  - invalid edits are rejected safely,
  - changes integrate with command/undo system.

Expected files:

```txt
Editor/include/SagaEditor/Panels/InspectorPanel.hpp
Editor/src/SagaEditor/Panels/InspectorPanel.cpp
```

---

### 9.3 Content Browser

- [ ] Add production-ready content browser.

  Done means:

  - project assets are visible,
  - folders can be browsed,
  - search/filter exists,
  - asset metadata is visible,
  - import/cook status is visible,
  - asset open actions route to appropriate editor surfaces.

Expected files:

```txt
Editor/include/SagaEditor/Panels/ContentBrowserPanel.hpp
Editor/src/SagaEditor/Panels/ContentBrowserPanel.cpp
```

---

### 9.4 Problems Panel

- [ ] Add problems/diagnostics panel.

  Done means:

  - validation errors are visible,
  - SDE diagnostics can be displayed,
  - asset import/cook errors can be displayed,
  - runtime preview errors can be displayed,
  - clicking a diagnostic navigates to relevant resource when possible.

Expected files:

```txt
Editor/include/SagaEditor/Panels/ProblemsPanel.hpp
Editor/src/SagaEditor/Panels/ProblemsPanel.cpp
```

---

### 9.5 Output Log Panel

- [ ] Add output/log panel.

  Done means:

  - editor logs are visible,
  - runtime preview logs are visible,
  - tool output can be streamed,
  - severity filters exist,
  - logs can be copied/exported.

Expected files:

```txt
Editor/include/SagaEditor/Panels/OutputLogPanel.hpp
Editor/src/SagaEditor/Panels/OutputLogPanel.cpp
```

---

## 10. Command System

- [ ] Provide a central editor command registry.

  Done means:

  - commands have stable ids,
  - commands have display names,
  - commands can declare shortcuts,
  - commands can be enabled/disabled by context,
  - commands can be invoked from menu, toolbar, shortcut, or panel.

Expected files:

```txt
Editor/include/SagaEditor/Commands/EditorCommand.hpp
Editor/include/SagaEditor/Commands/ICommandRegistry.hpp
Editor/src/SagaEditor/Commands/CommandRegistry.cpp
```

- [ ] Integrate commands with undo/redo.

  Done means:

  - mutating editor actions are represented as undoable operations,
  - undo stack is per project/workspace where appropriate,
  - redo works after undo,
  - command failure does not corrupt editor state.

Expected files:

```txt
Editor/include/SagaEditor/Commands/IUndoStack.hpp
Editor/include/SagaEditor/Commands/UndoableCommand.hpp
Editor/src/SagaEditor/Commands/UndoStack.cpp
```

- [ ] Add command palette.

  Done means:

  - user can search commands,
  - command availability is context-aware,
  - command execution routes through the registry,
  - hidden/debug commands can be gated.

---

## 11. Selection System

- [ ] Provide shared editor selection state.

  Done means:

  - hierarchy, inspector, viewport, and content browser observe the same selection model,
  - selection supports entities,
  - selection supports assets,
  - selection supports components or subobjects where needed,
  - selection changes are observable through editor services.

Expected files:

```txt
Editor/include/SagaEditor/Selection/SelectionState.hpp
Editor/include/SagaEditor/Selection/ISelectionService.hpp
Editor/src/SagaEditor/Selection/SelectionService.cpp
```

- [ ] Make selection collaboration-aware.

  Done means:

  - local selection remains local by default,
  - optional presence can expose active selection to collaborators,
  - remote selections are visually distinct,
  - remote selection display does not mutate local selection.

---

## 12. Viewport Tools

- [ ] Add editor viewport service boundary.

  Done means:

  - viewport can render project scene preview,
  - selection syncs with hierarchy/inspector,
  - camera navigation exists,
  - gizmo tools can operate on selected objects,
  - runtime preview can be started without changing authoring state unexpectedly.

Expected files:

```txt
Editor/include/SagaEditor/Viewport/IEditorViewport.hpp
Editor/include/SagaEditor/Viewport/ViewportTool.hpp
Editor/src/SagaEditor/Viewport/EditorViewport.cpp
```

- [ ] Add transform tools.

  Done means:

  - translate tool works,
  - rotate tool works,
  - scale tool works,
  - snapping can be configured,
  - transform edits integrate with undo/redo,
  - transform edits are blocked when collaboration lock/permission denies access.

- [ ] Add viewport diagnostics overlays.

  Done means overlays can show:

  - selected entity,
  - bounds,
  - physics debug,
  - navigation debug,
  - replication debug where relevant,
  - collaboration ownership indicators.

---

## 13. Asset Authoring

- [ ] Add asset import workflow.

  Done means:

  - user can import supported asset types,
  - imported assets receive stable ids,
  - import diagnostics are shown,
  - failed imports do not create corrupt project state,
  - imported assets appear in content browser.

Expected files:

```txt
Editor/include/SagaEditor/Assets/AssetImportRequest.hpp
Editor/include/SagaEditor/Assets/IAssetImportService.hpp
Editor/src/SagaEditor/Assets/AssetImportService.cpp
```

- [ ] Add asset reimport workflow.

  Done means:

  - source path is tracked when available,
  - asset can be reimported,
  - metadata is preserved where safe,
  - dependent resources are invalidated or refreshed,
  - errors are surfaced in Problems panel.

- [ ] Add asset cook/build integration.

  Done means:

  - editor can request asset cooking through proper service boundary,
  - cook status is visible,
  - failed cook blocks unsafe preview/publish where required,
  - editor does not own runtime streaming internals.

---

## 14. Scene Authoring

- [ ] Add scene editing workflow.

  Done means:

  - create scene,
  - open scene,
  - save scene,
  - duplicate scene,
  - rename scene,
  - delete scene,
  - validate scene before save/publish.

Expected files:

```txt
Editor/include/SagaEditor/Scene/SceneDocument.hpp
Editor/include/SagaEditor/Scene/ISceneDocumentService.hpp
Editor/src/SagaEditor/Scene/SceneDocumentService.cpp
```

- [ ] Add entity/component authoring.

  Done means:

  - entities can be created,
  - entities can be deleted,
  - components can be added,
  - components can be removed,
  - component fields can be edited,
  - invalid component states are rejected safely.

- [ ] Add prefab authoring.

  Done means:

  - entity selection can be saved as prefab,
  - prefab instance can be placed,
  - prefab overrides are visible,
  - prefab changes can be applied/reverted,
  - prefab editing integrates with collaboration locks/claims.

---

## 15. Visual Scripting and SDE Integration

- [ ] Integrate SDE graph outputs into editor workflows.

  Done means:

  - editor can display SDE graph resources,
  - editor can invoke SDE compile through a service boundary,
  - compile diagnostics appear in Problems panel,
  - failed SDE compile blocks unsafe preview/publish,
  - editor does not include SDE compiler internals.

- [ ] Add visual graph editor surface.

  Done means:

  - graph nodes are visible,
  - graph edges are visible,
  - graph editing is undoable,
  - graph validation is visible,
  - graph resource locks/claims are respected.

Expected files:

```txt
Editor/include/SagaEditor/Graphs/GraphEditorPanel.hpp
Editor/src/SagaEditor/Graphs/GraphEditorPanel.cpp
Editor/include/SagaEditor/Graphs/IGraphDocumentService.hpp
Editor/src/SagaEditor/Graphs/GraphDocumentService.cpp
```

- [ ] Keep SDE standalone.

  Forbidden dependency direction:

  ```txt
  SDE → Saga
  SDE → SagaEngine
  SDE → SagaEditor
  SDE → SagaServer
  SDE → SagaShared
  SDE → SagaCollaboration
  SDE → Forge
  SDE → Prism
  SDE → SagaTools
  ```

---

## 16. Runtime Preview

- [ ] Add runtime preview mode inside editor.

  Done means:

  - user can start runtime preview,
  - user can stop runtime preview,
  - preview uses runtime systems through approved boundary,
  - editor authoring state is protected,
  - preview logs and diagnostics are visible.

Expected files:

```txt
Editor/include/SagaEditor/Preview/IRuntimePreviewService.hpp
Editor/src/SagaEditor/Preview/RuntimePreviewService.cpp
```

- [ ] Add play/edit state separation.

  Done means:

  - editor clearly shows whether preview is running,
  - changes during preview are either isolated or explicitly applied,
  - stopping preview restores safe editor state,
  - runtime crash does not crash the editor shell where avoidable.

- [ ] Add multiplayer preview integration.

  Done means:

  - preview can launch client/server test sessions,
  - preview can consume stable session descriptors,
  - preview does not include server-private headers,
  - preview diagnostics show network/session state.

---

## 17. Editor Collaboration UX

- [ ] Add collaboration toolbar.

  Done means toolbar supports:

  - start session,
  - join session,
  - copy invite/room code,
  - leave session,
  - view participants,
  - view conflicts,
  - view locks,
  - view sync state.

Expected files:

```txt
Editor/include/SagaEditor/Collaboration/CollaborationToolbar.hpp
Editor/src/SagaEditor/Collaboration/CollaborationToolbar.cpp
```

- [ ] Add participants panel.

  Done means panel shows:

  - participant name,
  - connection state,
  - role,
  - current activity,
  - active resource,
  - stale state warnings.

Expected files:

```txt
Editor/include/SagaEditor/Collaboration/ParticipantsPanel.hpp
Editor/src/SagaEditor/Collaboration/ParticipantsPanel.cpp
```

- [ ] Add presence indicators.

  Done means indicators appear in:

  - hierarchy,
  - content browser,
  - inspector,
  - scene viewport,
  - graph editor,
  - asset tabs.

- [ ] Add lock and claim UI.

  Done means editor shows:

  - claimed resource badge,
  - locked resource badge,
  - owner display,
  - lock reason,
  - release request,
  - host/admin override where allowed.

- [ ] Add conflict resolution UI.

  Done means conflict UI supports:

  - affected resource list,
  - local change summary,
  - remote change summary,
  - conflict reason,
  - accept local,
  - accept remote,
  - manual resolve,
  - defer,
  - rollback where supported.

Expected files:

```txt
Editor/include/SagaEditor/Collaboration/ConflictResolutionPanel.hpp
Editor/src/SagaEditor/Collaboration/ConflictResolutionPanel.cpp
```

- [ ] Keep editor collaboration UI as a consumer of `SagaCollaboration`.

  Done means:

  - editor imports stable collaboration APIs,
  - editor does not define final collaboration contracts,
  - editor does not own session truth,
  - editor does not own collaboration persistence,
  - editor does not own collaboration transport.

---

## 18. Diagnostics

- [ ] Add editor diagnostics service.

  Done means:

  - diagnostics can be collected from editor systems,
  - diagnostics can reference project resources,
  - diagnostics have severity,
  - diagnostics can be cleared or refreshed,
  - panels can subscribe to diagnostics.

Expected files:

```txt
Editor/include/SagaEditor/Diagnostics/EditorDiagnostic.hpp
Editor/include/SagaEditor/Diagnostics/IEditorDiagnosticsService.hpp
Editor/src/SagaEditor/Diagnostics/EditorDiagnosticsService.cpp
```

- [ ] Integrate diagnostics with Problems panel.

  Done means Problems panel displays:

  - editor validation errors,
  - asset import errors,
  - SDE compile errors,
  - runtime preview errors,
  - collaboration errors,
  - publish-blocking errors.

- [ ] Add actionable diagnostic navigation.

  Done means clicking a diagnostic opens:

  - relevant asset,
  - relevant scene,
  - relevant graph,
  - relevant inspector field,
  - relevant log entry where available.

---

## 19. Settings and Preferences

- [ ] Add editor preferences service.

  Done means preferences support:

  - editor theme preference,
  - layout preference,
  - viewport settings,
  - shortcut settings,
  - tool settings,
  - collaboration display settings.

Expected files:

```txt
Editor/include/SagaEditor/Preferences/IEditorPreferences.hpp
Editor/src/SagaEditor/Preferences/EditorPreferences.cpp
```

- [ ] Add project-local editor settings.

  Done means:

  - workspace-specific editor settings can be stored,
  - user-global preferences are separate from project-local settings,
  - settings migration is versioned.

---

## 20. Menu and Toolbar System

- [ ] Add main menu model.

  Done means:

  - menu entries are command-backed,
  - menu availability is context-aware,
  - product-level menu ownership remains with Saga where applicable,
  - editor contributes authoring actions only.

Expected files:

```txt
Editor/include/SagaEditor/Menu/EditorMenuModel.hpp
Editor/src/SagaEditor/Menu/EditorMenuModel.cpp
```

- [ ] Add editor toolbar model.

  Done means:

  - toolbar entries are command-backed,
  - active tool state is visible,
  - context-sensitive actions can appear,
  - toolbar does not own product-level session lifecycle.

---

## 21. Project Integration

- [ ] Consume project descriptors from Saga product layer.

  Done means:

  - editor opens prepared project/workspace descriptors,
  - editor does not own project dashboard truth,
  - editor does not own recent project registry truth,
  - invalid descriptors fail with clear diagnostics.

- [ ] Add editor document model.

  Done means:

  - opened resources are tracked as documents,
  - dirty state is visible,
  - save/save all works,
  - close prompts are safe,
  - collaboration locks/permissions affect mutating document actions.

Expected files:

```txt
Editor/include/SagaEditor/Documents/EditorDocument.hpp
Editor/include/SagaEditor/Documents/IEditorDocumentService.hpp
Editor/src/SagaEditor/Documents/EditorDocumentService.cpp
```

---

## 22. Packaging and Extension Points

- [ ] Add editor extension registration model.

  Done means:

  - panels can be registered,
  - commands can be registered,
  - tools can be registered,
  - asset editors can be registered,
  - extension failures are isolated and diagnosed.

- [ ] Keep extension APIs stable and narrow.

  Done means extension APIs do not expose:

  - Qt implementation types,
  - product shell internals,
  - server-private headers,
  - runtime-private internals,
  - backend connection handles.

---

## 23. Testing Requirements

- [ ] Add unit tests for editor services.

  Required areas:

  - command registry,
  - undo stack,
  - selection service,
  - diagnostics service,
  - preferences service,
  - document service.

- [ ] Add integration tests for editor shell mounting.

  Done means:

  - editor mode can mount,
  - editor mode can unmount,
  - project close is safe,
  - layout restore does not crash,
  - missing panels are handled safely.

- [ ] Add boundary tests.

  Done means CI checks:

  - public editor headers do not include Qt directly,
  - editor does not include server-private headers,
  - runtime/server do not include editor-private collaboration headers,
  - SDE does not depend on editor headers.

---

## 24. CI and Architecture Enforcement

- [ ] Add public header include scanner.

  Required checks:

```txt
Editor/include/SagaEditor/** must not include Qt headers directly unless explicitly approved.
```

- [ ] Add forbidden dependency scanner.

  Required checks:

```txt
Editor/** must not include Server/src/**
Runtime/** must not include Editor/include/SagaEditor/Collaboration/**
Server/** must not include Editor/include/SagaEditor/Collaboration/**
SDE/** must not include SagaEditor/**
```

- [ ] Add CMake dependency validation.

  Done means invalid upward dependencies fail CI.

Bad examples:

```txt
SagaShared → SagaEditor
SagaShared → SagaCollaboration
Engine/Core → SagaEditor
Engine/Core → Apps/Saga
SDE → SagaEditor
```

---

## 25. Migration Plan

- [ ] Mark `Apps/Editor` as a development compatibility launcher.

  Done means documentation and CMake comments clearly identify it as non-production UX.

- [ ] Introduce Saga-owned editor mounting path.

  Done means `Saga` can mount editor mode through a stable interface.

- [ ] Move product workflow assumptions out of editor code.

  Done means project dashboard, recent projects, global session lifecycle, and mode switching are Saga-owned.

- [ ] Freeze editor-private collaboration contracts.

  Deprecated location:

```txt
Editor/include/SagaEditor/Collaboration
```

  Done means this path contains UI/adapters only, not final product collaboration contracts.

- [ ] Extract neutral collaboration contracts into `SagaShared`.

- [ ] Move collaboration implementation into `SagaCollaboration`.

- [ ] Convert editor collaboration panels to consume `SagaCollaboration` APIs.

---

## 26. Non-Goals

This roadmap does not own:

- Saga product shell,
- project dashboard product UX,
- runtime networking,
- server authority,
- gameplay replication policy,
- SDE compiler internals,
- Forge build frontend behavior,
- Prism code intelligence behavior,
- SagaTools dispatch behavior,
- backend infrastructure ownership,
- final collaboration implementation.

Related ownership:

| Area | Owner |
|---|---|
| Product shell | `Saga` |
| Runtime/server systems | `SagaEngine` / `SagaServer` |
| Shared contracts | `SagaShared` |
| Collaboration implementation | `SagaCollaboration` |
| SDE compiler | `SDE` |
| Tool ecosystem | `TOOLS_ROADMAP.md` |

---

## 27. Production Definition of Done

- [ ] Editor mode mounts inside Saga product executable.

- [ ] Editor public headers remain Qt-free.

- [ ] Qt implementation is hidden behind approved UI abstractions.

- [ ] Docking and layouts are stable.

- [ ] Core panels are production-ready.

- [ ] Command and undo systems are reliable.

- [ ] Selection state is shared across editor surfaces.

- [ ] Viewport tools are usable and undoable.

- [ ] Asset authoring workflows are safe.

- [ ] Scene authoring workflows are safe.

- [ ] SDE graph workflows are integrated through service boundaries.

- [ ] Runtime preview works without corrupting authoring state.

- [ ] Collaboration UI consumes `SagaCollaboration` and does not own collaboration truth.

- [ ] Diagnostics are actionable.

- [ ] CI enforces dependency boundaries.

- [ ] `Apps/Editor` remains development compatibility, not production architecture.

---

## 28. Final Architecture Rule

The editor architecture should remain:

```txt
Saga
  owns product lifecycle and primary executable

SagaEditor
  owns authoring UX and editor-local services

SagaShared
  owns neutral contracts

SagaCollaboration
  owns collaboration implementation

SagaEngine / Runtime
  owns game execution

SagaServer
  owns authority

SDE
  owns deterministic data compilation
```

The editor is an authoring module.

It is not the product shell.

It is not the collaboration backend.

It is not the server.

It is not the runtime.

This should be obvious, but software architecture exists because obvious things apparently need bodyguards.