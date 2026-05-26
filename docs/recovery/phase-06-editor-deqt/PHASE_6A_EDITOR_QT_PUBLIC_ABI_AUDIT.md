# Phase 6A Editor Qt Public ABI Audit

> Last updated: 2026-05-25
> Status: Phase 6A report-only audit
> Phase 6: Editor Public API De-Qtification

Phase 5 is closed as Package / Asset / Runtime Readiness Foundation
established. Phase 6 is open as Editor Public API De-Qtification.

Phase 6A is a report-only audit. It inventories the current Qt public ABI,
records Editor UI ownership boundaries, and selects the smallest safe Phase 6B
implementation slice. It does not remove Qt includes, move files, change CMake
behavior, or implement Phase 6B.

Full CTest remains unverified.

## Opening Summary

- Phase 5 is closed as Package / Asset / Runtime Readiness Foundation
  established.
- Phase 6 is open as Editor Public API De-Qtification.
- Phase 6A is accepted as a report-only Editor Qt public ABI and UI ownership
  audit.
- Phase 6A does not implement Qt removal, panel migration, file movement, CMake
  dependency conversion, or hard-fail enforcement.

## Qt Public ABI Inventory

| Path | Public/private | Qt symbol/type | Public API exposure | Consumer impact | Current owner | Desired owner | Risk | Recommended action | Classification |
|---|---|---|---|---|---|---|---|---|---|
| `Editor/include/SagaEditor/UI/Qt/QtPanel.h` | Public Editor header | `<QWidget>`, `QWidget`, `Q_OBJECT`, `QtPanel` | Yes | Consumers of `SagaEditorLib` must see Qt and moc-shaped ABI. | Editor public include root | Qt backend private/public-backend adapter only | High | Keep report-only in 6A; baseline as legacy leak; migrate behind neutral panel contract in later slices. | public ABI leak |
| `Editor/include/SagaEditor/Panels/GraphViewportPanel.h` | Public Editor header | `QtPanel` | Yes | Panel type cannot be included without Qt. | Editor public panels | Backend-neutral panel public contract plus Qt implementation | Medium | Convert after guard baseline; likely same pattern as Production Dashboard or Qt backend implementation. | public ABI leak |
| `Editor/include/SagaEditor/Panels/ProfilerPanel.h` | Public Editor header | `QtPanel` | Yes | Panel type cannot be included without Qt. | Editor public panels | Backend-neutral panel public contract plus Qt implementation | Medium | Convert after guard baseline. | public ABI leak |
| `Editor/include/SagaEditor/Panels/CollaborationPanel.h` | Public Editor header | `QtPanel` | Yes | Collaboration UI public header exposes Qt even though collaboration services are Qt-free. | Editor public panels | Collaboration service contract plus private Qt panel | Medium | Convert after collaboration UI boundary is documented. | public ABI leak |
| `Editor/include/SagaEditor/Panels/ProjectBrowserPanel.h` | Public Editor header | `QtPanel` | Yes | Product/editor consumers inherit Qt through a placeholder panel header. | Editor public panels | Backend-neutral panel public contract plus Qt implementation | Medium | Convert after guard baseline. | public ABI leak |
| `Editor/include/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h` | Public Editor header | `QtPanel` | Yes | Visual scripting UI classes publish Qt. | Visual scripting public headers | Visual scripting model/controller public contract; Qt view private | Medium | Defer until visual scripting panel ownership slice. | public ABI leak |
| `Editor/include/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.h` | Public Editor header | `QtPanel` | Yes | Visual scripting UI classes publish Qt. | Visual scripting public headers | Visual scripting model/controller public contract; Qt view private | Medium | Defer until visual scripting panel ownership slice. | public ABI leak |
| `Editor/include/SagaEditor/VisualScripting/Editor/WatchPanel.h` | Public Editor header | `QtPanel` | Yes | Visual scripting UI classes publish Qt. | Visual scripting public headers | Visual scripting model/controller public contract; Qt view private | Medium | Defer until visual scripting panel ownership slice. | public ABI leak |
| `Editor/include/SagaEditor/VisualScripting/Editor/BreakpointPanel.h` | Public Editor header | `QtPanel` | Yes | Visual scripting UI classes publish Qt. | Visual scripting public headers | Visual scripting model/controller public contract; Qt view private | Medium | Defer until visual scripting panel ownership slice. | public ABI leak |
| `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h` | Public Editor header | `QtPanel` | Yes | `GraphDocument` view surface is Qt-coupled. | Visual scripting public headers | Backend-neutral graph view contract plus Qt view implementation | Medium-high | Defer until graph UI surface is split deliberately. | public ABI leak |
| `Editor/include/SagaEditor/VisualScripting/Editor/NodePalette.h` | Public Editor header | `QtPanel` | Yes | Visual scripting UI classes publish Qt. | Visual scripting public headers | Visual scripting model/controller public contract; Qt view private | Medium | Defer until visual scripting panel ownership slice. | public ABI leak |
| `Editor/include/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.h` | Public Editor header | `QtPanel` | Yes | Visual scripting UI classes publish Qt. | Visual scripting public headers | Visual scripting model/controller public contract; Qt view private | Medium | Defer until visual scripting panel ownership slice. | public ABI leak |
| `Apps/Saga/SagaEditorModule.h` | Product public header | `QMainWindow`, `QStackedWidget` forward declarations | Yes | Product editor activation API is Qt-shaped. | Saga product shell | Product private Qt adapter or backend-neutral mount contract | Medium-high | Keep visible as Product boundary debt; do not fold into first Editor-only cleanup. | legacy/deferred exposure |
| `Editor/include/SagaEditor/UI/Qt/QtUIApplication.h` | Public Editor include root, Qt backend namespace | `QtUIApplication` name only; Qt hidden by pimpl | Limited | Consumers can choose the Qt backend type without seeing Qt headers. | Editor Qt backend | Approved Qt backend adapter | Low | Allow as backend adapter for now; revisit after backend include-root split. | allowed private Qt implementation |
| `Editor/include/SagaEditor/UI/Qt/QtUIMainWindow.h` | Public Editor include root, Qt backend namespace | `QtUIMainWindow` name only; Qt hidden by pimpl | Limited | Consumers can choose the Qt backend type without seeing Qt headers. | Editor Qt backend | Approved Qt backend adapter | Low | Allow as backend adapter for now; revisit after backend include-root split. | allowed private Qt implementation |
| `Editor/include/SagaEditor/UI/Qt/QtUIFactory.h` | Public Editor include root, Qt backend namespace | `QtUIFactory` name only | Limited | App launchers can instantiate Qt backend without Qt headers. | Editor Qt backend | Approved Qt backend adapter | Low | Allow as backend adapter. | allowed private Qt implementation |
| `Editor/include/SagaEditor/UI/Qt/QtSettingsStore.h` | Public Editor include root, Qt backend namespace | `QtSettingsStore` name only | Limited | Settings backend is selectable without Qt headers. | Editor Qt backend | Approved Qt backend adapter | Low | Allow as backend adapter. | allowed private Qt implementation |
| `Editor/src/SagaEditor/UI/Qt/**` | Private implementation | QtWidgets, QtCore, QtGui | No | Expected Qt implementation surface. | Editor Qt backend | Editor Qt backend | Low | Keep allowed. | allowed private Qt implementation |
| `Apps/Editor/**`, `Apps/EditorLab/**`, `Apps/SagaEditorComposer/**` static plugin/main files | App/private | Qt app/plugin setup | No for Editor public ABI | App launchers own executable composition. | App targets | App targets | Low | Keep allowed. | app-only exposure |
| `Apps/Saga/*.cpp` Qt usage | Product implementation | QtWidgets, process helpers, product shell | No for header ABI except `SagaEditorModule.h` | Product app is currently Qt-based. | Saga product shell | Saga product shell/private adapters | Medium | Keep out of Phase 6B Editor-only cleanup. | app-only exposure |
| `Tests/Unit/Editor/**` Qt references | Tests | test doubles, broad link graph | No production ABI | Current broad `UnitTests` target inherits editor dependencies. | Test graph | Narrower future test targets | Medium | Record as test graph debt; do not fix in 6A. | test-only exposure |
| Documentation references to Qt | Docs | prose only | No | Records known debt. | Docs | Docs | Low | Keep accurate. | false positive |

## Ownership Boundary Decisions

Qt ownership:

- QtWidgets is owned by the Editor Qt backend implementation and app launch or
  product composition code.
- `Editor/src/SagaEditor/UI/Qt/**` is the approved Editor Qt implementation
  location.
- App executable launchers may instantiate Qt application/backend objects.

Backend-neutral Editor contracts:

- `IPanel`, `PanelId`, `UIDockArea`, `IUIMainWindow`, `IUIApplication`,
  `IUIFactory`, `IUIWidget`, view models, diagnostics, customization models,
  profile/persona models, and collaboration service contracts should remain
  Qt-free.
- `void*` native handles are tolerated only as backend boundary handles. Code
  outside the active backend must not cast them.

EditorShell ownership:

- `EditorShell` owns panel registration, shell layout, profile/persona panel
  visibility, and command wiring through backend-neutral interfaces.
- `EditorShell` should not require concrete Qt panel types in public contracts.

Panel and layout ownership:

- Panel IDs and layout descriptors are Editor-owned and backend-neutral.
- Panel widgets belong to the active UI backend implementation.
- The Production Dashboard path is the current evidence for a Qt-free public
  panel header with private Qt widget implementation.

SDE customization ownership:

- SDE-driven customization belongs to Editor composition/customization services
  and generated composition artifacts.
- SDE parser, AST, semantic internals, and Qt widgets should not leak into the
  public customization model.

Collaboration ownership:

- Collaboration services and state remain Qt-free public Editor services.
- Collaboration UI belongs to a private Qt panel implementation or backend
  adapter. `CollaborationPanel.h` is a current public ABI leak because it
  inherits `QtPanel`.

Product/SagaEditor ownership:

- SagaEditor and EditorLab launchers may use Qt directly at the app boundary.
- `Apps/Saga/SagaEditorModule.h` is product public ABI debt because it exposes
  `QMainWindow` and `QStackedWidget`; it should not be corrected inside the
  first Editor-only implementation slice unless a Product boundary slice owns
  it.

Runtime UI overlap:

- RmlUi runtime UI ownership is separate from Editor Qt ownership. Existing
  RmlUi boundary tests keep RmlUi headers isolated to the backend
  implementation and do not justify Editor Qt public exposure.

## Test And CMake Inventory

| Target/test | Labels | What it proves | Broad/focused | Safe for 6A | Qt dependency | Split later |
|---|---|---|---|---|---|---|
| `UnitTests` / `SagaUnitTests` | `unit;runtime;server;networking;replication;asset;editor` | Broad unit coverage including Editor tests. | Broad | Inventory only | Yes, through `SagaEditorLib` and broad test graph | Yes |
| `SagaEditorCompositionTests` | `tools;sde;editor;editor-composition` when SDE is enabled | Composition compiler/editor composition behavior. | Focused tool/editor | Inventory only | Yes, through `SagaEditorLib` | Maybe after Editor public ABI cleanup |
| `SagaEditorCompositionCompilerHelp` | `tools;sde;editor;editor-composition` when SDE executable exists | Tool help smoke. | Focused | Inventory only | App/tool dependent | No immediate split |
| `SagaPackageStagingTests` | `unit;product;package` | Product package staging. | Focused | Not Phase 6 evidence | Product Qt through product target possible | No Phase 6 split |
| `SagaPublishReadinessTests` | `unit;product;package;asset` | Publish readiness package/asset/identity report. | Focused | Not Phase 6 evidence | Product target possible | No Phase 6 split |
| `SagaPipelineTests` | `tools;integration` | Pipeline integration/tool behavior. | Medium | Inventory only | No direct Editor Qt claim | No Phase 6 split |
| `IntegrationTests` | `integration;runtime;server;networking;replication;timing-sensitive` | Runtime/server integration. | Broad | Inventory only | Not Editor focused | No Phase 6 split |
| `ReplicationTests` | `replication;integration;slow;long-running` | Replication behavior. | Broad/heavy | Inventory only | Not Editor focused | No Phase 6 split |

Current label inventory from the configured build:

- `editor`: 1 test (`UnitTests`)
- `ui`: 0 tests
- `integration`: 3 tests (`SagaPipelineTests`, `IntegrationTests`,
  `ReplicationTests`)
- `product`: 2 tests (`SagaPackageStagingTests`,
  `SagaPublishReadinessTests`)
- `collaboration`: 0 tests

The editor test label is broad because `UnitTests` groups many subsystems. That
is useful for coverage, but it is not proof of a clean Editor public ABI.

CMake boundary evidence:

- `SagaEditorLib` exports `Editor/include` and links `Qt6::Core` and
  `Qt6::Widgets` as `PUBLIC`. This remains correct while public Editor headers
  expose Qt types.
- `SagaEditorLabLib` directly links Qt privately, but it publicly links
  `SagaEditorLib`, so it still inherits Editor public Qt ABI exposure.
- `SagaProductLib` remains a broader product dependency hub and is outside the
  first Editor-only implementation slice.

## Phase 6B Candidate Slices

| Candidate | Goal | Bundle contents | Likely files | Behavior | Tests | Risk | Value | Why now | Why later | Rollback |
|---|---|---|---|---|---|---|---|---|---|---|
| A. Editor Public Header Qt Exposure Report / Boundary Guard | Make Qt exposure measurable and prevent new leaks. | Architecture test or report with explicit allowlist for current leaks; docs update. | `Tests/Unit/Architecture`, docs, maybe `cmake/modules/SagaTests.cmake` only if a focused target already exists. | Guard/report only; no runtime UI behavior. | Architecture test plus targeted `rg`. | Low | High | Smallest enforceable baseline before removals. | If the team requires actual ABI reduction first. | Remove guard/test and docs entry. |
| B. Backend-Neutral Editor Panel Handle Contract | Reduce direct reliance on `QtPanel` by formalizing neutral panel handles. | Add or refine neutral panel adapter contract around `IPanel` and native widget ownership. | `Editor/include/SagaEditor/Panels`, selected panel implementation. | API addition or small migration. | Focused Editor/unit tests. | Medium | High | Builds on existing `IPanel` path. | Needs guard first to avoid migration drift. | Revert added contract and panel change. |
| C. EditorShell Panel Visibility API De-Qtification | Ensure shell visibility/control never requires concrete Qt types. | Add neutral visibility methods or adapters for EditorLab/scenario use. | `EditorShell`, EditorLab adapters/tests. | Small API change. | EditorLab scenario tests. | Medium | Medium | Useful if EditorLab is the blocker. | Current shell surface already mostly neutral. | Revert API addition and tests. |
| D. EditorLab Scenario Adapter Qt Boundary Hardening | Keep EditorLab scenario runtime adapters on neutral shell/host services. | Audit and tighten scenario panel adapter ownership. | `Apps/EditorLab`, tests/docs. | Minimal behavior. | `EditorLabScenarioRunnerTests`. | Low-medium | Medium | Protects dev tooling. | Does not directly reduce public Qt ABI. | Revert adapter changes. |
| E. Collaboration Panel Qt Boundary Audit | Split collaboration service contract from collaboration panel widget contract. | Document or migrate collaboration panel to pimpl/private Qt implementation. | Collaboration panel header/source, collaboration tests. | Possible ABI change. | Collaboration/editor tests. | Medium | Medium | Current `CollaborationPanel.h` is a leak. | Needs general panel pattern first. | Revert panel migration. |
| F. Editor Public API Facade / Private Qt Adapter Split | Establish broad facade and move Qt backend behind private adapters. | New facade headers and backend implementation split. | Many Editor headers/sources. | Broad API change. | Broad Editor, app, product tests. | High | High | End-state direction. | Too broad for 6B. | Large revert. |
| G. CMake Qt Dependency Visibility Cleanup | Convert `SagaEditorLib` Qt links from `PUBLIC` to `PRIVATE`. | CMake dependency conversion after public headers are clean. | `cmake/modules/SagaTargets.cmake`. | Build interface change. | Build all Editor consumers. | High now | High later | Necessary after cleanup. | Unsafe while public headers expose Qt. | One-edge CMake revert. |

## Recommended Phase 6B

Select Candidate A: Editor Public Header Qt Exposure Report / Boundary Guard.

Decision:

- Prefer the smallest slice that makes public Qt exposure enforceable without
  breaking EditorLab, SagaEditor, collaboration, or SDE customization.
- Do not begin broad Qt removal in Phase 6B.
- Do not move files in Phase 6B.
- Do not convert `SagaEditorLib` Qt links to `PRIVATE` until public headers no
  longer expose Qt.
- Do not include `Apps/Saga/SagaEditorModule.h` cleanup in the first Editor-only
  slice; keep it visible as Product boundary debt.

Suggested Phase 6B acceptance:

- A deterministic architecture test or boundary report scans public Editor
  headers for Qt includes/types.
- Existing leaks are listed in a narrow allowlist.
- New non-allowlisted Qt exposure in public Editor headers fails the focused
  architecture check.
- The guard is report/enforcement for boundary expansion only; it does not
  claim Editor public ABI is Qt-free.

## Non-Claims

Phase 6A is not:

- broad Qt removal
- editor architecture rewrite
- SDE customization rewrite
- collaboration rewrite
- UI/document `AssetKind` implementation
- runtime/client asset work
- publish gate work
- MultiplayerSandbox work
- full CTest or full test health

Phase 6A does not claim:

- Editor public headers are Qt-free
- `SagaEditorLib` can publish Qt privately
- Product editor activation is backend-neutral
- EditorLab is free of inherited Editor Qt ABI
- collaboration UI is backend-neutral

## Verification

Pre-audit checks:

- `git status --short` showed a broadly dirty recovery worktree with existing
  modified and untracked files.
- `git diff --check` was clean before Phase 6A docs edits.
- Targeted `rg` scans found Qt exposure in public Editor headers, allowed Qt
  backend implementation paths, product app Qt usage, and existing CMake
  `Qt6::Core`/`Qt6::Widgets` public links.
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L editor` listed
  `UnitTests`.
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L ui` listed 0 tests.
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L integration` listed
  `SagaPipelineTests`, `IntegrationTests`, and `ReplicationTests`.
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L product` listed
  `SagaPackageStagingTests` and `SagaPublishReadinessTests`.
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L collaboration` listed 0
  tests.

Required docs-only verification after this checkpoint:

- `git diff --check`
- targeted `rg` for Phase 6A wording, Qt exposure classifications, selected
  Phase 6B slice, non-goals, and full CTest unverified statement
- CTest label inventory only for `editor`, `ui`, `integration`, `product`, and
  optionally `collaboration`

Full CTest remains unverified unless explicitly run.
