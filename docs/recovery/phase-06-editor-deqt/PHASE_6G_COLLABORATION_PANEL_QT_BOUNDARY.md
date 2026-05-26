# Phase 6G CollaborationPanel Qt Boundary

> Last updated: 2026-05-25
> Status: Phase 6G bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6G closes Phase 6F honestly, performs the collaboration panel ownership
audit, and migrates exactly one additional public panel header away from
`QtPanel` inheritance. The selected panel is `CollaborationPanel`.

Full CTest remains unverified.

## Phase 6F Closure

Phase 6F is accepted as:

- third simple panel pimpl migration
- `ProjectBrowserPanel` public Qt exposure removal
- repeated `IPanel` + pimpl/private Qt widget pattern proof
- Phase 6B allowlist reduction

Phase 6F is not accepted as:

- broad Qt removal
- `QtPanel` removal
- `CollaborationPanel` migration
- visual scripting migration
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup
- project/workspace ownership redesign

Full CTest remains unverified.

Completion evidence:

- `ProjectBrowserPanel` public header is Qt-free.
- `ProjectBrowserPanel` now inherits `IPanel`.
- Its private Qt implementation exists under
  `Editor/src/SagaEditor/UI/Qt/Panels`.
- The `ProjectBrowserPanel.h` allowlist entry was removed.
- The boundary guard passed with 0 violations during Phase 6F verification.
- `SagaEditorLib`, `SagaArchitectureTests`, focused architecture/editor tests,
  and `UnitTests` passed during Phase 6F verification.

Phase 6F proves:

- the `IPanel` + pimpl pattern is repeatable for simple panels
- public Qt exposure can be reduced incrementally
- allowlist reduction is working
- migration can continue to the last simple editor panel after ownership audit

Phase 6F does not prove:

- `CollaborationPanel` is migrated.
- Visual scripting public headers are clean.
- `QtPanel` itself is removable.
- `SagaEditorModule` product ABI is clean.
- CMake Qt PUBLIC visibility can change.
- Project or workspace ownership can move.

## CollaborationPanel Ownership Audit

`CollaborationPanel` was inspected before editing.

| Question | Result |
|---|---|
| Public header Qt exposure | `QtPanel` include/inheritance only |
| Meaningful implementation logic | No; the source file was a stub |
| Collaboration session/network ownership | None in the panel source |
| Document sync/lock/permission/presence/conflict ownership | None in the panel source |
| Extra public Qt types beyond `QtPanel` | None |
| EditorShell registration assumption | Standard `IPanel` registration through stable panel id |
| QWidget/QObject behavior beyond native widget | None found |
| Existing focused panel test | None |
| Migration safety | Safe to use the Graph/Profiler/ProjectBrowser pattern |

The wider repo has collaboration ownership constraints: collaboration truth
belongs in collaboration services and shared contracts, not in the editor panel
shell. This migration keeps `CollaborationPanel` display-only and does not wire
session, networking, document sync, lock, permission, presence, or conflict
state.

The stable panel id is `saga.panel.collaboration`; the title is
`Collaboration`.

Rollback:

- restore `CollaborationPanel.h` to inherit `QtPanel`
- remove `Editor/src/SagaEditor/UI/Qt/Panels/CollaborationPanel.cpp`
- restore the `CollaborationPanel.h` allowlist entry
- remove this checkpoint and docs references

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/Panels/CollaborationPanel.h` no longer includes
  `SagaEditor/UI/Qt/QtPanel.h`.
- `CollaborationPanel` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public class name and default constructor are preserved.
- The header now exposes the existing backend-neutral native handle method:
  `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/Panels/CollaborationPanel.cpp` owns the private
  Qt widget through `CollaborationPanel::Impl`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.collaboration`.
- `GetTitle()` returns `Collaboration`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The old `Editor/src/SagaEditor/Panels/CollaborationPanel.cpp` is a
  non-defining moved stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/Panels/CollaborationPanel.h` from the allowlist.
- All visual scripting public Qt leaks remain unchanged.
- `QtPanel.h` remains temporary allowlisted debt.

## Remaining Public Qt Leaks

Still allowlisted after Phase 6G:

- `Editor/include/SagaEditor/UI/Qt/QtPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h`
- `Editor/include/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/WatchPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/BreakpointPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/NodePalette.h`
- `Editor/include/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.h`
- `Apps/Saga/SagaEditorModule.h`

## Non-Goals

Phase 6G does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate visual scripting UI
- change `SagaEditorModule`
- rewrite `EditorShell`
- redesign collaboration session, networking, document sync, lock, permission,
  presence, or conflict ownership
- change `SagaEditorLib` Qt dependency visibility
- split editor tests
- rewrite SDE customization
- implement UI/document `AssetKind`
- change runtime/client asset work
- change publish gates
- change MultiplayerSandbox
- prove full CTest health

## Verification

Verification completed for this slice:

- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R '^UnitTests$' --output-on-failure -j 1`
- `git diff --check`
- targeted `rg` for `CollaborationPanel`, migrated simple panels, `QtPanel`,
  public Qt exposure, architecture allowlist, Phase 6G, `IPanel`, pimpl, and
  `Collaboration`
- CTest label inventory for `architecture`, `editor`, `ui`, `product`,
  `collaboration`, and `integration`

`EditorQtPublicAbiBoundaryTests` reported 288 scanned public headers, 35
allowed legacy Qt exposure findings, and 0 violations.

No focused behavior test was added. Constructing the migrated panel creates a
Qt widget and the current safe unit seams do not provide a narrow
QApplication-backed panel construction target.

Recommended Phase 6H: plan the visual scripting leaf panel cluster, because the
simple editor-panel cluster is cleared and the remaining public Qt leaks are
visual scripting headers, `QtPanel.h`, and `SagaEditorModule.h`.
