# Phase 6B Editor Qt Public ABI Boundary Guard

> Last updated: 2026-05-25
> Status: Phase 6B bounded enforcement/report slice
> Phase 6: Editor Public API De-Qtification

Phase 6B implements the selected Phase 6A follow-up: an Editor public header
Qt exposure boundary guard. It makes the current public Qt ABI leaks explicit
and fails on new public Qt exposure outside the current allowlist.

This is not Qt removal.

Full CTest remains unverified.

## Phase 6A Closure

Phase 6A is accepted as:

- Editor Qt public ABI / Editor UI ownership audit
- public Qt exposure inventory
- Qt ownership boundary decision
- Phase 6B boundary guard recommendation

Phase 6A is not accepted as:

- Qt removal
- public Editor API de-Qtification implementation
- panel migration
- visual scripting UI migration
- `SagaEditorModule` API de-Qtification
- CMake Qt visibility cleanup
- editor test split
- full CTest health

## Guard Scope

The guard lives in `Tests/Unit/Architecture/EditorQtPublicAbiBoundaryTests.cpp`
and runs inside the existing `SagaArchitectureTests` executable. That target
uses `SAGA_SOURCE_ROOT` to scan repository files and does not link Qt.

Scanned files:

- `Editor/include/**/*.h|hpp|hh|hxx`
- `Apps/Saga/SagaEditorModule.h`

Ignored by scope:

- `Editor/src/SagaEditor/UI/Qt/**` private implementation files
- app `.cpp` composition files
- tests
- documentation prose

Detected public Qt exposure tokens:

- Qt includes such as `#include <Q...>` and `#include <Qt...>`
- quoted Qt includes
- `Q_OBJECT`
- concrete Qt API names including `QObject`, `QWidget`, `QMainWindow`,
  `QStackedWidget`, `QString`, `QVariant`, `QModelIndex`, `QAction`, `QMenu`,
  `QDockWidget`, `QApplication`, `QTreeView`, `QTableView`, and `QAbstract*`
- `QtCore`, `QtGui`, `QtWidgets`, and `Qt::`
- `QtPanel`
- `SagaEditor/UI/Qt/QtPanel.h`

Comment-only matches are ignored.

## Current Allowlist

The allowlist is path-exact and token-specific. A file is allowed only for its
current known exposure category.

| Path | Allowed tokens |
|---|---|
| `Editor/include/SagaEditor/UI/Qt/QtPanel.h` | `QtInclude`, `Q_OBJECT`, `QWidget`, `QtPanel` |
| `Editor/include/SagaEditor/Panels/GraphViewportPanel.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/Panels/ProfilerPanel.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/Panels/CollaborationPanel.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/Panels/ProjectBrowserPanel.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/VisualScripting/Editor/WatchPanel.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/VisualScripting/Editor/BreakpointPanel.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/VisualScripting/Editor/NodePalette.h` | `QtPanelInclude`, `QtPanel` |
| `Editor/include/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.h` | `QtPanelInclude`, `QtPanel` |
| `Apps/Saga/SagaEditorModule.h` | `QMainWindow`, `QStackedWidget` |

The guard also treats stale allowlist entries as failures. When a later slice
removes a leak, that slice must update the allowlist.

## What Fails Now

The focused CTest entry fails when:

- a public Editor header gains a Qt include or Qt type outside the allowlist
- `Apps/Saga/SagaEditorModule.h` exposes a new Qt token beyond its current
  `QMainWindow` and `QStackedWidget` forward declarations
- an allowlisted file disappears from the scan scope
- an allowlisted token is no longer observed and the allowlist was not updated

The test output reports:

- total scanned headers
- allowed legacy leak count
- violation count
- all allowlist entries
- violations with file, line, token, and source line
- a reminder that Phase 6B is a guard, not Qt removal

## CTest Entry

`cmake/modules/SagaTests.cmake` registers:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1
```

The entry runs:

```sh
SagaArchitectureTests --gtest_filter=EditorQtPublicAbiBoundaryTests.*
```

Labels:

- `unit`
- `architecture`
- `editor`

## Non-Goals

Phase 6B does not:

- remove Qt
- remove `QtPanel`
- migrate panels
- rewrite visual scripting UI
- change `SagaEditorModule`
- change `EditorShell`
- change `SagaEditorLib` Qt link visibility
- split editor tests
- rewrite SDE customization
- rewrite collaboration UI
- implement UI/document `AssetKind`
- change runtime/client asset work
- change publish gates
- change MultiplayerSandbox
- prove full CTest health

## Next Slice

Recommended Phase 6C: QtPanel Public ABI Replacement Plan.

Rationale: `QtPanel` is the root public leak for multiple panel and visual
scripting headers. A replacement plan should precede individual panel
migrations so each migration follows the same backend-neutral contract.

Full CTest remains unverified.
