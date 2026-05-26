# Phase 6F ProjectBrowserPanel pimpl Migration

> Last updated: 2026-05-25
> Status: Phase 6F bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6F closes Phase 6E honestly and migrates exactly one additional public
panel header away from `QtPanel` inheritance. The selected panel is
`ProjectBrowserPanel`.

Full CTest remains unverified.

## Phase 6E Closure

Phase 6E is accepted as:

- second low-risk panel pimpl migration
- `ProfilerPanel` public Qt exposure removal
- repeated `IPanel` + pimpl/private Qt widget pattern proof
- Phase 6B allowlist reduction

Phase 6E is not accepted as:

- broad Qt removal
- `QtPanel` removal
- `ProjectBrowserPanel` migration
- `CollaborationPanel` migration
- visual scripting migration
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup

Full CTest remains unverified.

Completion evidence:

- `ProfilerPanel` public header is Qt-free.
- `ProfilerPanel` now inherits `IPanel`.
- Its private Qt implementation exists under
  `Editor/src/SagaEditor/UI/Qt/Panels`.
- The `ProfilerPanel.h` allowlist entry was removed.
- The boundary guard passed with 0 violations during Phase 6E verification.
- `SagaEditorLib`, `SagaArchitectureTests`, focused architecture/editor tests,
  and `UnitTests` passed during Phase 6E verification.

Phase 6E proves:

- the `IPanel` + pimpl pattern is repeatable for simple panels
- public Qt exposure can be reduced incrementally
- allowlist reduction is working
- migration can continue to the next simple panel after inspection

Phase 6E does not prove:

- `ProjectBrowserPanel` or `CollaborationPanel` are migrated.
- Visual scripting public headers are clean.
- `QtPanel` itself is removable.
- `SagaEditorModule` product ABI is clean.
- CMake Qt PUBLIC visibility can change.

## ProjectBrowserPanel Inspection

`ProjectBrowserPanel` was inspected before editing.

| Question | Result |
|---|---|
| Public header Qt exposure | `QtPanel` include/inheritance only |
| Meaningful implementation logic | No; the source file was a stub |
| Project/workspace/file-tree/model dependency | None in the editor panel source |
| Extra public Qt types beyond `QtPanel` | None |
| EditorShell registration assumption | Standard `IPanel` registration through stable panel id |
| QWidget/QObject behavior beyond native widget | None found |
| Existing focused panel test | None |
| Migration safety | Safe to use the `GraphViewportPanel`/`ProfilerPanel` pattern |

The stable panel id is `saga.panel.project`; the title is `Project Browser`.
No project/workspace ownership behavior was added or redesigned.

Rollback:

- restore `ProjectBrowserPanel.h` to inherit `QtPanel`
- remove `Editor/src/SagaEditor/UI/Qt/Panels/ProjectBrowserPanel.cpp`
- restore the `ProjectBrowserPanel.h` allowlist entry
- remove this checkpoint and docs references

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/Panels/ProjectBrowserPanel.h` no longer includes
  `SagaEditor/UI/Qt/QtPanel.h`.
- `ProjectBrowserPanel` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public class name and default constructor are preserved.
- The header now exposes the existing backend-neutral native handle method:
  `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/Panels/ProjectBrowserPanel.cpp` owns the private
  Qt widget through `ProjectBrowserPanel::Impl`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.project`.
- `GetTitle()` returns `Project Browser`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The old `Editor/src/SagaEditor/Panels/ProjectBrowserPanel.cpp` is a
  non-defining moved stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/Panels/ProjectBrowserPanel.h` from the allowlist.
- All other Phase 6B allowlisted leaks remain unchanged.
- `QtPanel.h` remains temporary allowlisted debt.

## Remaining Public Qt Leaks

Still allowlisted after Phase 6F:

- `Editor/include/SagaEditor/UI/Qt/QtPanel.h`
- `Editor/include/SagaEditor/Panels/CollaborationPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h`
- `Editor/include/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/WatchPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/BreakpointPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/NodePalette.h`
- `Editor/include/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.h`
- `Apps/Saga/SagaEditorModule.h`

## Non-Goals

Phase 6F does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate more than one panel
- migrate `CollaborationPanel`
- migrate visual scripting UI
- change `SagaEditorModule`
- rewrite `EditorShell`
- redesign project/workspace ownership
- change `SagaEditorLib` Qt dependency visibility
- split editor tests
- rewrite SDE customization
- rewrite collaboration UI
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
- targeted `rg` for `ProjectBrowserPanel`, `ProfilerPanel`,
  `GraphViewportPanel`, `QtPanel`, public Qt exposure, architecture allowlist,
  Phase 6F, `IPanel`, and pimpl wording
- CTest label inventory for `architecture`, `editor`, `ui`, `product`,
  `collaboration`, and `integration`

`EditorQtPublicAbiBoundaryTests` reported 288 scanned public headers, 38
allowed legacy Qt exposure findings, and 0 violations.

No focused behavior test was added. Constructing the migrated panel creates a
Qt widget and the current safe unit seams do not provide a narrow
QApplication-backed panel construction target.

Recommended Phase 6G: perform a `CollaborationPanel` migration audit before
implementation because it is the last simple editor-panel allowlist entry but
may carry collaboration service ownership risk.
