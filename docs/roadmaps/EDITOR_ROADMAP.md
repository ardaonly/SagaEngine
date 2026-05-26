# SagaEditor — Authoring Module Roadmap



---

## 0. Roadmap Convention



An editor that owns product lifecycle because it was convenient is not an editor.

It is a product shell pretending to be a panel.

---

## 1. Document Purpose

This document is the source of truth for SagaEditor authoring systems.

SagaEditor is the authoring module mounted by Saga. It owns the user-facing authoring experience around scenes, assets, entities, graphs, scripts, diagnostics, previews, and collaboration display.

It covers:



SagaEditor is not the primary product executable.

The production product direction is:

```txt
Saga = primary user-facing executable
SagaEditor = authoring module/mode mounted by Saga
Apps/Editor = optional development compatibility launcher
```

This distinction is required for clean product ownership.

Correct model:



Incorrect model:

```txt
Editor opens projects, owns sessions, compiles data, runs the server, cooks assets, and publishes packages because the button was nearby.
```

That is how authoring tools become architectural gravity wells.

---

## 2. Companion Documents



---

## 3. Product Ownership Rule

* [x] Define SagaEditor as an authoring module instead of the primary product executable.

  Represented by:

  ```txt
  EDITOR_ROADMAP.md
  DependencyGraph.md
  COLLABORATION_ROADMAP.md
  SAGA_PRODUCT_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  Saga owns product lifecycle.
  SagaEditor owns authoring UX.
  ```

* [ ] Mount editor mode inside the Saga-owned product process.

  Done means:

  * `Saga` owns `QApplication` in production Qt mode,
  * `Saga` owns the primary main window,
  * editor mode mounts into a Saga-owned surface,
  * editor mode does not create a second product process lifecycle,
  * editor mode can unmount cleanly,
  * closing a project returns to the Saga product shell,
  * `Apps/Editor` remains optional and development-only.

* [ ] Remove production assumptions that treat `Apps/Editor` as the main user workflow.

  Done means:

  * user-facing project creation starts in `Saga`,
  * user-facing project opening starts in `Saga`,
  * user-facing mode switching is owned by `Saga`,
  * editor-only launcher paths are marked as development compatibility,
  * editor code does not own recent project registry truth.

---

## 4. Editor Ownership

* [x] Define editor ownership boundaries.

  Represented by:

  ```txt
  EDITOR_ROADMAP.md
  DependencyGraph.md
  SAGA_PRODUCT_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  Editor owns authoring UX.
  Editor does not own product shell, global session truth, runtime authority, or server authority.
  ```

* [ ] Keep the editor responsible for authoring UI only.

  Done means the editor owns:

  * panels,
  * docking,
  * editor layouts,
  * viewport tools,
  * hierarchy,
  * inspector,
  * content browser,
  * asset authoring surfaces,
  * graph authoring UI,
  * script authoring UX,
  * generated code preview UX,
  * editor commands,
  * editor-local preferences,
  * editor diagnostics panels,
  * collaboration display panels,
  * conflict resolution UI,
  * editor-local service adapters.

* [ ] Prevent editor code from owning product-level workflows.

  Done means the editor does not own:

  * Saga product shell,
  * project dashboard,
  * project creation as a product workflow,
  * project opening as a product workflow,
  * recent project registry truth,
  * session lifecycle truth,
  * final collaboration contracts,
  * runtime authority,
  * server authority,
  * process-level product orchestration,
  * build/publish pipeline truth.

---

## 5. Dependency Rules

### 5.1 Allowed Dependencies

Allowed examples:



Editor may consume stable public contracts and service APIs.

Editor must not include private implementation because a panel needed one more field.

---

### 5.2 Forbidden Dependencies

Forbidden examples:



Forbidden shortcuts:



---

## 6. Qt Boundary

* [x] Keep public SagaEditor UI headers Qt-free through UI abstraction interfaces.

  Represented by:

  ```txt
  Editor/include/SagaEditor/UI/IUIApplication.h
  Editor/include/SagaEditor/UI/IUIFactory.h
  Editor/include/SagaEditor/UI/IUIMainWindow.h
  Editor/include/SagaEditor/UI/IUIWidget.h
  ```

  Preserved decision:

  ```txt
  Qt is an implementation backend.
  SagaEditor public UI-facing contracts remain backend-neutral.
  ```

* [ ] Keep Qt implementation behind backend/private boundaries.

  Done means:

  * public editor headers do not expose Qt types,
  * Qt widgets stay in implementation files or approved Qt backend folders,
  * editor panels communicate through SagaEditor interfaces,
  * product integration does not require consumers to include Qt headers.

  Phase 6 progress:

  * `GraphViewportPanel`, `ProfilerPanel`, `ProjectBrowserPanel`,
    `CollaborationPanel`, `WatchPanel`, `BreakpointPanel`, and
    `ExecutionTraceView` have moved from public `QtPanel` inheritance to
    `IPanel` + private Qt pimpl implementations.
  * Remaining public Qt exposure is tracked by
    `EditorQtPublicAbiBoundaryTests` and the Phase 6 architecture notes.
  * `GraphDebuggerView`, `NodePalette`, `NodeLibraryPanel`, `GraphCanvas`,
    `QtPanel.h`, and `SagaEditorModule.h` remain unresolved boundary debt.

* [ ] Enforce Qt-free public headers through CI.

  Done means CI rejects public headers under:

  ```txt
  Editor/include/SagaEditor/**
  ```

  if they include Qt headers directly, except explicitly approved backend boundary headers.

* [ ] Keep `QApplication` ownership outside the editor module in production.

  Done means:

  * production `QApplication` is created by `Saga`,
  * editor mode receives an application/window abstraction,
  * editor mode does not own process lifetime,
  * `Apps/Editor` compatibility launcher is isolated from production architecture.

---

## 7. Editor Shell

* [ ] Provide a production editor shell mounted inside Saga.

  Done means:

  * editor shell can mount into Saga product window,
  * editor shell can unmount cleanly,
  * project close returns to Saga product shell,
  * layout state is restored per project/workspace,
  * editor shell has no hidden dependency on standalone launcher ownership,
  * editor shell can receive prepared workspace/project descriptors from Saga.

Expected files:

```txt
Editor/include/SagaEditor/Shell/EditorShell.h
Editor/include/SagaEditor/Shell/EditorMode.h
Editor/include/SagaEditor/Shell/EditorMountContext.h
Editor/include/SagaEditor/Shell/EditorUnmountResult.h
Editor/src/SagaEditor/Shell/EditorShell.cpp
Editor/src/SagaEditor/Shell/EditorMode.cpp
```

* [ ] Keep development launcher separate from production editor shell.

  Done means:

  * `Apps/Editor` can launch editor for local development,
  * `Apps/Editor` does not define product architecture,
  * production users enter editor through `Saga`,
  * any launcher-only project open behavior is marked compatibility/test-only.

Expected files:

```txt
Apps/Editor/main.cpp
Apps/Saga/main.cpp
```

---

## 8. Docking and Layout System

* [ ] Provide dockable editor panels.

  Done means:

  * panels can be docked,
  * panels can be undocked,
  * panels can be hidden and restored,
  * layout persists per workspace/user preference,
  * layout reset is available,
  * docking system does not leak Qt types into public editor contracts.

Expected files:

```txt
Editor/include/SagaEditor/Docking/DockWorkspace.h
Editor/include/SagaEditor/Docking/DockLayoutManager.h
Editor/include/SagaEditor/Docking/DockTab.h
Editor/src/SagaEditor/Docking/DockWorkspace.cpp
Editor/src/SagaEditor/Docking/DockLayoutManager.cpp
```

* [ ] Support named editor layouts.

  Required layouts:

  ```txt
  DefaultLayout
  BeginnerLayout
  IntermediateLayout
  AdvancedGraphLayout
  CSharpGameplayLayout
  AssetAuthoringLayout
  NetworkDebugLayout
  ToolDeveloperLayout
  TeamDashboardLayout
  DiagnosticsLayout
  ```

* [ ] Add layout migration/versioning.

  Done means:

  * old layout files do not crash the editor,
  * missing panels are ignored safely,
  * renamed panels are migrated or skipped with diagnostics,
  * profile-specific layout changes do not destroy hidden advanced state.

---

## 9. Authoring Profiles Integration

* [ ] Consume authoring profile descriptors.

  Done means editor layout, visible panels, block palettes, diagnostics verbosity, generated code visibility, and authority detail level can be selected from authoring profile descriptors.

Required profiles:

```txt
Beginner
Intermediate
Advanced
CSharpGameplay
NetworkDeveloper
ToolDeveloper
TeamLead
DiagnosticEngineer
```

Expected files:

```txt
Editor/include/SagaEditor/Authoring/ProfileLayoutResolver.h
Editor/include/SagaEditor/Authoring/ProfilePanelVisibility.h
Editor/include/SagaEditor/Authoring/ProfileBlockPaletteFilter.h
Editor/include/SagaEditor/Authoring/ProfileDiagnosticPresenter.h
Editor/src/SagaEditor/Authoring/ProfileLayoutResolver.cpp
Editor/src/SagaEditor/Authoring/ProfilePanelVisibility.cpp
Editor/src/SagaEditor/Authoring/ProfileBlockPaletteFilter.cpp
Editor/src/SagaEditor/Authoring/ProfileDiagnosticPresenter.cpp
```

* [ ] Keep profiles separate from permissions.

  Done means:

  * profile controls UX visibility/depth,
  * collaboration permissions control what actions are allowed,
  * advanced profile does not bypass team permissions,
  * beginner profile does not hide publish-blocking errors.

* [ ] Add profile-safe mode switching.

  Done means switching profiles:

  * does not corrupt graph/script/asset state,
  * does not delete hidden advanced data,
  * warns before irreversible graph-to-script conversions,
  * preserves user layout preferences where safe.

---

## 10. Panel Framework

* [ ] Define a standard panel lifecycle.

  Done means panels support:

  * creation,
  * mounting,
  * activation,
  * deactivation,
  * refresh,
  * serialization of local UI state,
  * clean destruction,
  * profile-based visibility.

Expected files:

```txt
Editor/include/SagaEditor/Panels/IPanel.h
Editor/include/SagaEditor/Panels/PanelDescriptor.h
Editor/include/SagaEditor/Extensions/ExtensionPanelBridge.h
Editor/src/SagaEditor/Extensions/ExtensionPanelBridge.cpp
```

* [ ] Provide a panel registry.

  Done means:

  * panels register through descriptors,
  * duplicate panel ids are rejected,
  * panel construction is centralized,
  * panels can be discovered by shell/docking/profile systems,
  * panels can declare authoring profile visibility.

* [ ] Keep panel implementation behind editor module boundaries.

  Done means:

  * panels do not own product lifecycle,
  * panels do not directly own backend connections,
  * panels do not import server-private headers,
  * panels do not define shared collaboration contracts,
  * panels consume diagnostics/services instead of owning global truth.

---

## 11. Core Editor Panels

### 11.1 Hierarchy Panel

* [ ] Add production-ready hierarchy panel.

  Done means:

  * scene entities are displayed,
  * parent/child relationships are visible,
  * selection syncs with inspector and viewport,
  * entity create/delete/rename actions exist,
  * invalid entity state is displayed clearly,
  * collaboration claims/locks are visible where relevant.

Expected files:

```txt
Editor/include/SagaEditor/Panels/HierarchyPanel.h
Editor/src/SagaEditor/Panels/HierarchyPanel.cpp
Editor/src/SagaEditor/UI/Qt/Panels/HierarchyPanel.cpp
```

---

### 11.2 Inspector Panel

* [ ] Add production-ready inspector panel.

  Done means:

  * selected entity/resource properties are visible,
  * editable fields support validation,
  * component data can be edited,
  * invalid edits are rejected safely,
  * changes integrate with command/undo system,
  * authority/replication/persistence metadata can be surfaced for graph/script-backed properties where applicable.

Expected files:

```txt
Editor/include/SagaEditor/Panels/InspectorPanel.h
Editor/src/SagaEditor/Panels/InspectorPanel.cpp
Editor/src/SagaEditor/UI/Qt/Panels/InspectorPanel.cpp
```

---

### 11.3 Content Browser

* [ ] Add production-ready content browser.

  Done means:

  * project assets are visible,
  * folders can be browsed,
  * search/filter exists,
  * asset metadata is visible,
  * import/cook status is visible,
  * stale artifact state is visible,
  * asset open actions route to appropriate editor surfaces,
  * collaboration claims/locks are displayed.

Expected files:

```txt
Editor/include/SagaEditor/Panels/AssetBrowserPanel.h
Editor/src/SagaEditor/Panels/AssetBrowserPanel.cpp
Editor/src/SagaEditor/UI/Qt/Panels/AssetBrowserPanel.cpp
```

---

### 11.4 Problems Panel

* [ ] Add problems/diagnostics panel.

  Done means Problems panel displays:



Expected files:

```txt
Editor/include/SagaEditor/Panels/ProblemsPanel.h
Editor/src/SagaEditor/Panels/ProblemsPanel.cpp
```

* [ ] Support layered diagnostic presentation.

  Done means the same diagnostic can be shown as:

  ```txt
  Simple
  Standard
  Technical
  RawPayload
  ```

  according to authoring profile.

---

### 11.5 Output Log Panel

* [ ] Add output/log panel.

  Done means:



Expected files:

```txt
Editor/include/SagaEditor/Panels/ConsolePanel.h
Editor/src/SagaEditor/Panels/ConsolePanel.cpp
Editor/src/SagaEditor/UI/Qt/Panels/ConsolePanel.cpp
```

---

### 11.6 Build and Publish Panel

* [ ] Add build/publish status panel.

  Done means editor can display product/build pipeline state without owning the pipeline:

  * selected build profile,
  * current build step,
  * validation/cook/compile/package progress,
  * diagnostics count,
  * artifact output summary,
  * publish readiness,
  * publish blockers,
  * rerun validation/build actions routed through Saga/Forge.

Expected files:

```txt
Editor/include/SagaEditor/Panels/BuildPublishPanel.h
Editor/src/SagaEditor/Panels/BuildPublishPanel.cpp
```

---

### 11.7 Diagnostics Report Panel

* [ ] Add diagnostics report panel.

  Done means technical profiles can inspect:



Expected files:

```txt
Editor/include/SagaEditor/Panels/DiagnosticsReportPanel.h
Editor/src/SagaEditor/Panels/DiagnosticsReportPanel.cpp
```

---

## 12. Command System

* [ ] Provide a central editor command registry.

  Done means:

  * commands have stable ids,
  * commands have display names,
  * commands can declare shortcuts,
  * commands can be enabled/disabled by context,
  * commands can be invoked from menu, toolbar, shortcut, command palette, or panel.

Expected files:

```txt
Editor/include/SagaEditor/Commands/CommandRegistry.h
Editor/include/SagaEditor/Commands/CommandDispatcher.h
Editor/include/SagaEditor/Commands/ShortcutManager.h
Editor/src/SagaEditor/Commands/CommandRegistry.cpp
Editor/src/SagaEditor/Commands/CommandDispatcher.cpp
Editor/src/SagaEditor/Commands/ShortcutManager.cpp
```

* [ ] Integrate commands with undo/redo.

  Done means:

  * mutating editor actions are represented as undoable operations,
  * undo stack is per project/workspace/document where appropriate,
  * redo works after undo,
  * command failure does not corrupt editor state,
  * graph/script/asset commands declare whether they are undoable.

Expected files:

```txt
Editor/include/SagaEditor/Commands/UndoRedoStack.h
Editor/src/SagaEditor/Commands/UndoRedoStack.cpp
```

* [ ] Add command palette.

  Done means:

  * user can search commands,
  * command availability is context-aware,
  * command execution routes through registry,
  * hidden/debug commands can be gated by profile and permissions.

---

## 13. Selection System

* [ ] Provide shared editor selection state.

  Done means:

  * hierarchy, inspector, viewport, graph editor, content browser, and asset inspectors observe the same selection model where applicable,
  * selection supports entities,
  * selection supports assets,
  * selection supports components/subobjects,
  * selection supports graph nodes/pins/edges,
  * selection changes are observable through editor services.

Expected files:

```txt
Editor/include/SagaEditor/Selection/SelectionManager.h
Editor/include/SagaEditor/Selection/SelectionFilter.h
Editor/include/SagaEditor/Selection/SelectionHistory.h
Editor/src/SagaEditor/Selection/SelectionManager.cpp
```

* [ ] Add selection history.

  Done means users can navigate previous selections and diagnostics can select affected resources.

---

## 14. Viewport and Scene Authoring

* [ ] Add production viewport tools.

  Done means editor supports:

  * view navigation,
  * selection,
  * transform gizmos,
  * snapping,
  * camera controls,
  * tool overlays,
  * runtime preview overlay separation.

Expected files:

```txt
Editor/include/SagaEditor/Viewport/ViewportPanel.h
Editor/include/SagaEditor/Viewport/ViewportTool.h
Editor/include/SagaEditor/Viewport/TransformGizmo.h
Editor/src/SagaEditor/Viewport/ViewportPanel.cpp
Editor/src/SagaEditor/Viewport/TransformGizmo.cpp
```

* [ ] Add scene editing workflow.

  Done means:

  * create scene,
  * open scene,
  * save scene,
  * duplicate scene,
  * rename scene,
  * delete scene,
  * validate scene before save/preview/publish,
  * respect collaboration claims/locks.

Expected files:

```txt
Editor/include/SagaEditor/Scene/SceneDocument.h
Editor/include/SagaEditor/Scene/ISceneDocumentService.h
Editor/src/SagaEditor/Scene/SceneDocumentService.cpp
```

* [ ] Add entity/component authoring.

  Done means:

  * entities can be created,
  * entities can be deleted,
  * components can be added,
  * components can be removed,
  * component fields can be edited,
  * invalid component states are rejected safely,
  * edits integrate with undo/redo and collaboration state.

* [ ] Add prefab authoring.

  Done means:

  * entity selection can be saved as prefab,
  * prefab instance can be placed,
  * prefab overrides are visible,
  * prefab changes can be applied/reverted,
  * prefab editing integrates with collaboration locks/claims,
  * prefab diagnostics are source-linked.

---

## 15. Asset Authoring UX

* [ ] Integrate asset pipeline with editor UX.

  Done means editor can display and edit asset pipeline-facing state while not owning runtime loading truth.

* [ ] Add controlled import workflow.

  Done means users can:

  * import source assets,
  * choose importer where needed,
  * edit import settings,
  * preview import result,
  * see import diagnostics,
  * commit/cancel import.

Expected files:

```txt
Editor/include/SagaEditor/Assets/ImportService.h
Editor/include/SagaEditor/Assets/AssetMetadataService.h
Editor/include/SagaEditor/Assets/AssetPreviewService.h
Editor/src/SagaEditor/Assets/ImportService.cpp
Editor/src/SagaEditor/Assets/AssetMetadataService.cpp
Editor/src/SagaEditor/Assets/AssetPreviewService.cpp
```

* [ ] Add reimport workflow.

  Done means users can:

  * detect source changes,
  * reimport assets,
  * preserve stable `AssetId`,
  * update imported metadata,
  * invalidate stale cooked artifacts,
  * trigger recook through Forge/asset pipeline boundary.

* [ ] Add asset inspector workflow.

  Done means users can inspect:

  * asset id,
  * asset kind,
  * source path,
  * import settings,
  * cook settings,
  * dependencies,
  * cooked artifact refs,
  * runtime memory estimate,
  * diagnostics.

* [ ] Keep runtime streaming separate from import UX.

  Done means editor does not force runtime streaming to load arbitrary source files in shipping/runtime mode.

---





  Done means:



* [ ] Add gameplay graph editor surface.

  Done means:

  * graph nodes are visible,
  * graph pins are visible,
  * graph edges are visible,
  * graph editing is undoable,
  * graph validation is visible,
  * graph resource locks/claims are respected,
  * source maps allow diagnostics to select graph nodes/pins/edges,
  * graph artifact freshness is visible.

Expected files:

```txt
Editor/include/SagaEditor/Graphs/GraphEditorPanel.h
Editor/include/SagaEditor/Graphs/IGraphDocumentService.h
Editor/include/SagaEditor/Graphs/GraphInspectorPanel.h
Editor/include/SagaEditor/Graphs/GraphDiagnosticAdapter.h
Editor/src/SagaEditor/Graphs/GraphEditorPanel.cpp
Editor/src/SagaEditor/Graphs/GraphDocumentService.cpp
Editor/src/SagaEditor/Graphs/GraphInspectorPanel.cpp
Editor/src/SagaEditor/Graphs/GraphDiagnosticAdapter.cpp
```

* [ ] Add block palette panel.

  Done means block palette:

  * lists blocks by category,
  * filters blocks by authoring profile,
  * hides dangerous/internal blocks from beginner profiles,
  * shows authority/replication/persistence badges where profile allows,
  * supports search,
  * respects package/plugin block availability.

Expected files:

```txt
Editor/include/SagaEditor/Graphs/BlockPalettePanel.h
Editor/include/SagaEditor/Graphs/BlockPaletteFilter.h
Editor/src/SagaEditor/Graphs/BlockPalettePanel.cpp
Editor/src/SagaEditor/Graphs/BlockPaletteFilter.cpp
```

* [ ] Support high-level block expansion UX.

  Done means users can:

  * inspect high-level block expansion,
  * convert high-level block to lower-level graph where allowed,
  * collapse graph regions into macros/subgraphs,
  * see migration warnings for deprecated expansions.



  Forbidden dependency direction:



---

## 17. Authority-Aware Authoring UX

* [ ] Display authority metadata in graph/script authoring.

  Done means editor can show:

  * graph authority context,
  * block authority requirements,
  * execution domain,
  * side effects,
  * replication effects,
  * persistence effects,
  * prediction safety,
  * security boundary.

Expected files:

```txt
Editor/include/SagaEditor/Authority/AuthorityBadgePresenter.h
Editor/include/SagaEditor/Authority/AuthorityDiagnosticAdapter.h
Editor/include/SagaEditor/Authority/SafeAuthorityFlowActions.h
Editor/src/SagaEditor/Authority/AuthorityBadgePresenter.cpp
Editor/src/SagaEditor/Authority/AuthorityDiagnosticAdapter.cpp
Editor/src/SagaEditor/Authority/SafeAuthorityFlowActions.cpp
```

* [ ] Block or diagnose invalid authority placement.

  Done means:

  * server-only blocks cannot silently run in client-only graphs,
  * persistent writes from client-only graphs are rejected,
  * prediction-unsafe blocks are rejected in predicted graphs,
  * editor-only/build-only blocks are rejected in runtime graphs,
  * diagnostics suggest safe request/validation flows.

Example diagnostic:

```txt
Inventory.GiveItem cannot run in ClientOnly graph HUD.OnButtonClicked.
Use SendServerRequest → ServerValidated Inventory Request.
```

* [ ] Add safe flow generation actions.

  Done means editor can scaffold:

  * client request graph,
  * server validation graph,
  * server authoritative handler graph,
  * response/replication UI graph where applicable.

---

## 18. C# Scripting UX

* [ ] Add script editor integration.

  Done means users can:

  * create C# scripts,
  * open/edit C# scripts,
  * view script diagnostics,
  * navigate symbols where Prism data is available,
  * trigger validate/compile through Forge/script service boundary,
  * inspect script artifact status.

Expected files:

```txt
Editor/include/SagaEditor/Scripting/ScriptEditorPanel.h
Editor/include/SagaEditor/Scripting/ScriptDiagnosticAdapter.h
Editor/src/SagaEditor/Scripting/ScriptEditorPanel.cpp
Editor/src/SagaEditor/Scripting/ScriptDiagnosticAdapter.cpp
```

* [ ] Add block binding inspector.

  Done means users can inspect:

  * `[BlockCallable]` methods,
  * block category/name,
  * parameter mapping,
  * return mapping,
  * authority metadata,
  * replication/persistence effects,
  * prediction safety,
  * binding diagnostics.

Expected files:

```txt
Editor/include/SagaEditor/Scripting/BlockBindingInspectorPanel.h
Editor/src/SagaEditor/Scripting/BlockBindingInspectorPanel.cpp
```

* [ ] Add generated C# preview panel.

  Done means graph users can view generated C# without treating generated code as source truth.

Expected files:

```txt
Editor/include/SagaEditor/Scripting/GeneratedCodePreviewPanel.h
Editor/src/SagaEditor/Scripting/GeneratedCodePreviewPanel.cpp
```

* [ ] Add explicit conversion UX.

  Required operations:

  ```txt
  View generated C#
  Convert graph to C# script
  Expose C# method as block
  Create custom script node
  Detach generated code as editable script
  ```

* [ ] Warn about non-reversible conversion.

  Done means editor clearly warns when converting graph to freeform C# may prevent clean reconstruction back into visual blocks.

---

## 19. Runtime and Client/Server Preview

* [ ] Add runtime preview mode inside editor/product flow.

  Done means:

  * user can start runtime preview,
  * user can stop runtime preview,
  * preview uses runtime systems through approved boundary,
  * preview consumes editor-preview artifacts/manifests,
  * editor authoring state is protected,
  * preview logs and diagnostics are visible.

Expected files:

```txt
Editor/include/SagaEditor/Preview/IRuntimePreviewService.h
Editor/include/SagaEditor/Preview/RuntimePreviewPanel.h
Editor/src/SagaEditor/Preview/RuntimePreviewService.cpp
Editor/src/SagaEditor/Preview/RuntimePreviewPanel.cpp
```

* [ ] Add play/edit state separation.

  Done means:

  * editor clearly shows whether preview is running,
  * changes during preview are isolated or explicitly applied,
  * stopping preview restores safe editor state,
  * runtime crash does not crash editor shell where avoidable,
  * stale/missing preview artifacts fail clearly.

* [ ] Add multiplayer preview integration.

  Done means:

  * preview can launch client/server test sessions through Saga/product/runtime boundaries,
  * preview consumes stable session descriptors,
  * preview does not include server-private headers,
  * preview diagnostics show network/session state,
  * server authority diagnostics are visible.

---

## 20. Build and Publish UX Integration

* [ ] Integrate editor with product build/publish workflow.

  Done means editor can trigger or display build/publish workflows through Saga/Forge boundaries, without owning the build pipeline.

* [ ] Show build profile and artifact status.

  Done means editor can show:



* [ ] Show publish blockers.

  Done means editor can show blockers such as:

  * graph validation errors,
  * authority validation errors,
  * script compile errors,
  * stale cooked assets,
  * invalid package manifests,
  * collaboration conflicts/locks,
  * permission failures.

* [ ] Route build/publish diagnostics to resources.

  Done means clicking a build/publish diagnostic can open:



---

## 21. Editor Collaboration UX

* [ ] Add collaboration toolbar.

  Done means toolbar supports:

  * session status,
  * participant summary,
  * connection state,
  * claim/lock indicators,
  * conflict indicators,
  * leave session action routed through Saga/SagaCollaboration.

* [ ] Add participants panel.

  Done means panel shows:

  * active participants,
  * display names,
  * roles/permissions summary,
  * connection state,
  * current activity where appropriate,
  * idle/disconnected state.

* [ ] Add resource claim/lock indicators.

  Done means editor displays claims/locks for:



* [ ] Add conflict resolution UI.

  Done means editor can display and route conflicts for:



* [ ] Keep editor as collaboration consumer.

  Done means editor does not own:

  * session truth,
  * final collaboration contracts,
  * transport implementation,
  * backend sync,
  * team membership source of truth.

---

## 22. Diagnostics

* [ ] Add editor diagnostics service.

  Done means:

  * diagnostics can be collected from editor systems,
  * diagnostics can reference project resources,
  * diagnostics have severity,
  * diagnostics can be cleared/refreshed,
  * panels can subscribe to diagnostics,
  * diagnostics can bridge external tool/runtime reports into editor presentation.

Expected files:

```txt
Editor/include/SagaEditor/Diagnostics/EditorDiagnostic.h
Editor/include/SagaEditor/Diagnostics/IEditorDiagnosticsService.h
Editor/include/SagaEditor/Diagnostics/DiagnosticNavigation.h
Editor/src/SagaEditor/Diagnostics/EditorDiagnosticsService.cpp
Editor/src/SagaEditor/Diagnostics/DiagnosticNavigation.cpp
```

* [ ] Integrate diagnostics with Problems panel.

  Required diagnostic families:



* [ ] Add actionable diagnostic navigation.

  Done means clicking a diagnostic opens:



---

## 23. Settings and Preferences

* [ ] Add editor preferences service.

  Done means preferences support:

  * editor theme preference,
  * layout preference,
  * authoring profile preference,
  * viewport settings,
  * shortcut settings,
  * tool settings,
  * diagnostics verbosity,
  * generated code preview preference,
  * collaboration display settings.

Expected files:

```txt
Editor/include/SagaEditor/Preferences/IEditorPreferences.h
Editor/src/SagaEditor/Preferences/EditorPreferences.cpp
```

* [ ] Add project-local editor settings.

  Done means:

  * workspace-specific editor settings can be stored,
  * user-global preferences are separate from project-local settings,
  * profile overrides are explicit,
  * settings migration is versioned.

---

## 24. Menu and Toolbar System

* [ ] Add main menu model.

  Done means:

  * menu entries are command-backed,
  * menu availability is context-aware,
  * product-level menu ownership remains with Saga where applicable,
  * editor contributes authoring actions only,
  * profile/permission visibility is respected.

Expected files:

```txt
Editor/include/SagaEditor/Menu/EditorMenuModel.h
Editor/src/SagaEditor/Menu/EditorMenuModel.cpp
```

* [ ] Add editor toolbar model.

  Done means:

  * toolbar entries are command-backed,
  * active tool state is visible,
  * context-sensitive actions can appear,
  * collaboration state can be surfaced,
  * toolbar does not own product-level session lifecycle.

---

## 25. Project and Workspace Integration

* [ ] Consume project/workspace descriptors from Saga product layer.

  Done means:

  * editor opens prepared project/workspace descriptors,
  * editor does not own project dashboard truth,
  * editor does not own recent project registry truth,
  * invalid descriptors fail with clear diagnostics,
  * editor receives build/profile/collaboration context from Saga.

* [ ] Add editor document model.

  Done means:

  * opened resources are tracked as documents,
  * dirty state is visible,
  * save/save all works,
  * close prompts are safe,
  * collaboration locks/permissions affect mutating document actions,
  * graph/script/asset/scene documents share consistent dirty/diagnostic behavior.

Expected files:

```txt
Editor/include/SagaEditor/Documents/EditorDocument.h
Editor/include/SagaEditor/Documents/IEditorDocumentService.h
Editor/include/SagaEditor/Documents/DocumentKind.h
Editor/src/SagaEditor/Documents/EditorDocumentService.cpp
```

---

## 26. Packaging and Extension Points

* [ ] Add editor extension registration model.

  Done means:

  * panels can be registered,
  * commands can be registered,
  * tools can be registered,
  * asset editors can be registered,
  * graph tools can be registered,
  * script tools can be registered,
  * extension failures are isolated and diagnosed.

* [ ] Keep extension APIs stable and narrow.

  Done means extension APIs do not expose:



* [ ] Support profile-aware extensions.

  Done means extensions can declare which authoring profiles they target, without bypassing permissions or ownership boundaries.

---

## 27. Testing Requirements

### 27.1 Header Boundary Tests

* [ ] Enforce public editor header boundaries.

  Required coverage:



---

### 27.2 Shell and Mount Tests

* [ ] Add editor shell mount/unmount tests.

  Required coverage:

  * editor mounts into Saga-provided surface,
  * editor unmounts cleanly,
  * project close returns to product shell,
  * standalone `Apps/Editor` path remains development-only.

---

### 27.3 Panel and Layout Tests

* [ ] Add panel/docking/layout tests.

  Required coverage:

  * panel registry rejects duplicates,
  * panel lifecycle runs cleanly,
  * layout restore works,
  * layout migration handles missing panels,
  * profile layouts hide/show correct panels.

---

### 27.4 Graph and Authority UX Tests

* [ ] Add graph/authority editor tests.

  Required coverage:

  * graph nodes/edges/pins display,
  * block palette filters by profile,
  * invalid authority block placement diagnosed,
  * safe server request flow action appears,
  * graph diagnostics navigate to node/pin/edge.

---

### 27.5 Scripting UX Tests

* [ ] Add scripting editor tests.

  Required coverage:

  * script diagnostics appear,
  * generated C# preview displays source mapping,
  * block binding inspector displays metadata,
  * non-reversible conversion warns,
  * C# profile exposes scripting surfaces while beginner profile hides them.

---

### 27.6 Asset UX Tests

* [ ] Add asset authoring tests.

  Required coverage:

  * content browser shows import/cook status,
  * asset inspector shows metadata/artifacts,
  * reimport invalidates cooked artifact,
  * stale artifact diagnostic appears,
  * asset diagnostics navigate to source/metadata.

---

### 27.7 Build/Publish UX Tests

* [ ] Add build/publish editor integration tests.

  Required coverage:

  * build status panel receives Forge state,
  * publish blockers display,
  * graph/script/asset blockers navigate correctly,
  * editor does not own build pipeline internals.

---

### 27.8 Collaboration UX Tests

* [ ] Add collaboration UI tests.

  Required coverage:

  * participants panel displays session state,
  * resource locks show on assets/graphs/scripts/scenes,
  * conflict records route to correct resources,
  * editor consumes SagaCollaboration services instead of owning session truth.

---

## 28. MVP Vertical Slice

The first editor vertical slice should prove the new authoring model without trying to implement every editor feature.

Required scenario:

```txt
Open MMO Starter project in Saga → mount editor → edit quest reward graph → validate authority → view generated C# preview → import one texture → build editor-preview → run local client/server preview → show publish blockers or ready state
```

Required behavior:

1. Saga opens project and passes workspace descriptor to editor.
2. Editor mounts inside Saga shell.
3. Authoring profile selects `Advanced` or `NetworkDeveloper` layout.
4. Graph editor opens `QuestReward` graph.
5. Block palette shows profile-filtered blocks.
6. Invalid client-side `GiveItem` placement produces authority diagnostic.
7. Editor suggests server-validated request flow.
8. Generated C# preview displays graph output read-only.
9. Content browser imports one texture and shows cook/artifact status.
10. Forge editor-preview build status appears in Build/Publish panel.
11. Runtime/client-server preview launches through approved boundary.
12. Problems panel shows graph/script/asset/build diagnostics with navigation.
13. Publish readiness panel shows exact blockers or ready state.

This slice proves SagaEditor is a real authoring module in a product pipeline.

Not a standalone GUI demo.

---

## 29. Non-Goals

SagaEditor must not become:



Editor owns authoring UX.

That is already a large enough job.

---

## 30. Risk Register

### 30.1 Risk: Editor Becomes Product Shell Again

Mitigation:

* Saga owns product shell and mode switching,
* editor mounts/unmounts through explicit API,
* Apps/Editor remains development compatibility,
* project create/open remains product-owned.

---

### 30.2 Risk: Graph Editor Owns Runtime Truth

Mitigation:



---

### 30.3 Risk: Beginner Profile Hides Critical Errors

Mitigation:

* diagnostics can be simplified but not suppressed when publish-blocking,
* technical details are expandable,
* publish readiness uses full validation regardless of profile.

---

### 30.4 Risk: Script Editor Bypasses Authority Rules

Mitigation:

* script diagnostics route through analyzer/build pipeline,
* block-callable bindings require metadata,
* editor displays metadata but does not decide safety alone.

---

### 30.5 Risk: Asset Preview Diverges From Runtime Package Truth

Mitigation:

* preview uses editor-preview/runtime-ready artifacts where possible,
* stale artifacts are diagnosed,
* runtime package manifests remain source of runtime load truth.

---

### 30.6 Risk: Collaboration UI Owns Session Truth

Mitigation:

* editor consumes SagaCollaboration services,
* neutral contracts live in SagaShared,
* final collaboration state is not stored under editor-private headers.

---

## 31. Suggested File Targets

Expected shell files:

```txt
Editor/include/SagaEditor/Shell/EditorShell.h
Editor/include/SagaEditor/Shell/EditorMode.h
Editor/include/SagaEditor/Shell/EditorMountContext.h
Editor/src/SagaEditor/Shell/EditorShell.cpp
Editor/src/SagaEditor/Shell/EditorMode.cpp
```

Expected profile files:

```txt
Editor/include/SagaEditor/Authoring/ProfileLayoutResolver.h
Editor/include/SagaEditor/Authoring/ProfilePanelVisibility.h
Editor/include/SagaEditor/Authoring/ProfileBlockPaletteFilter.h
Editor/include/SagaEditor/Authoring/ProfileDiagnosticPresenter.h
Editor/src/SagaEditor/Authoring/ProfileLayoutResolver.cpp
Editor/src/SagaEditor/Authoring/ProfilePanelVisibility.cpp
Editor/src/SagaEditor/Authoring/ProfileBlockPaletteFilter.cpp
Editor/src/SagaEditor/Authoring/ProfileDiagnosticPresenter.cpp
```

Expected graph files:

```txt
Editor/include/SagaEditor/Graphs/GraphEditorPanel.h
Editor/include/SagaEditor/Graphs/IGraphDocumentService.h
Editor/include/SagaEditor/Graphs/GraphInspectorPanel.h
Editor/include/SagaEditor/Graphs/BlockPalettePanel.h
Editor/include/SagaEditor/Graphs/GraphDiagnosticAdapter.h
Editor/src/SagaEditor/Graphs/GraphEditorPanel.cpp
Editor/src/SagaEditor/Graphs/GraphDocumentService.cpp
Editor/src/SagaEditor/Graphs/GraphInspectorPanel.cpp
Editor/src/SagaEditor/Graphs/BlockPalettePanel.cpp
Editor/src/SagaEditor/Graphs/GraphDiagnosticAdapter.cpp
```

Expected authority files:

```txt
Editor/include/SagaEditor/Authority/AuthorityBadgePresenter.h
Editor/include/SagaEditor/Authority/AuthorityDiagnosticAdapter.h
Editor/include/SagaEditor/Authority/SafeAuthorityFlowActions.h
Editor/src/SagaEditor/Authority/AuthorityBadgePresenter.cpp
Editor/src/SagaEditor/Authority/AuthorityDiagnosticAdapter.cpp
Editor/src/SagaEditor/Authority/SafeAuthorityFlowActions.cpp
```

Expected scripting files:

```txt
Editor/include/SagaEditor/Scripting/ScriptEditorPanel.h
Editor/include/SagaEditor/Scripting/GeneratedCodePreviewPanel.h
Editor/include/SagaEditor/Scripting/BlockBindingInspectorPanel.h
Editor/include/SagaEditor/Scripting/ScriptDiagnosticAdapter.h
Editor/src/SagaEditor/Scripting/ScriptEditorPanel.cpp
Editor/src/SagaEditor/Scripting/GeneratedCodePreviewPanel.cpp
Editor/src/SagaEditor/Scripting/BlockBindingInspectorPanel.cpp
Editor/src/SagaEditor/Scripting/ScriptDiagnosticAdapter.cpp
```

Expected asset files:

```txt
Editor/include/SagaEditor/Assets/ImportService.h
Editor/include/SagaEditor/Assets/AssetMetadataService.h
Editor/include/SagaEditor/Assets/AssetPreviewService.h
Editor/include/SagaEditor/Assets/AssetInspectorPanel.h
Editor/src/SagaEditor/Assets/ImportService.cpp
Editor/src/SagaEditor/Assets/AssetMetadataService.cpp
Editor/src/SagaEditor/Assets/AssetPreviewService.cpp
Editor/src/SagaEditor/Assets/AssetInspectorPanel.cpp
```

Expected build/publish files:

```txt
Editor/include/SagaEditor/Panels/BuildPublishPanel.h
Editor/include/SagaEditor/Panels/DiagnosticsReportPanel.h
Editor/src/SagaEditor/Panels/BuildPublishPanel.cpp
Editor/src/SagaEditor/Panels/DiagnosticsReportPanel.cpp
```

Expected diagnostics files:

```txt
Editor/include/SagaEditor/Diagnostics/EditorDiagnostic.h
Editor/include/SagaEditor/Diagnostics/IEditorDiagnosticsService.h
Editor/include/SagaEditor/Diagnostics/DiagnosticNavigation.h
Editor/src/SagaEditor/Diagnostics/EditorDiagnosticsService.cpp
Editor/src/SagaEditor/Diagnostics/DiagnosticNavigation.cpp
```

---

## 32. Decision Summary

Preserve these decisions:



SagaEditor's job is to make a complex MMO-first engine authorable without making the architecture dishonest.

That is the line.

Cross it, and the editor becomes the place where correctness goes to die with nice icons.
