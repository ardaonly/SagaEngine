# Phase 6E ProfilerPanel pimpl Migration

> Last updated: 2026-05-25
> Status: Phase 6E bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6E closes Phase 6D honestly and migrates exactly one additional public
panel header away from `QtPanel` inheritance. The selected panel is
`ProfilerPanel`.

Full CTest remains unverified.

## Phase 6D Closure

Phase 6D is accepted as:

- first low-risk panel pimpl migration
- `GraphViewportPanel` public Qt exposure removal
- `IPanel` + pimpl/private Qt widget pattern proof
- Phase 6B allowlist reduction

Phase 6D is not accepted as:

- broad Qt removal
- `QtPanel` removal
- multiple panel migration
- visual scripting migration
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup

Full CTest remains unverified.

Completion evidence:

- `GraphViewportPanel` public header is Qt-free.
- `GraphViewportPanel` now inherits `IPanel`.
- Its private Qt implementation exists under
  `Editor/src/SagaEditor/UI/Qt/Panels`.
- The old public allowlist entry was removed.
- The boundary guard passed with 0 violations during Phase 6D verification.
- `SagaEditorLib`, `SagaArchitectureTests`, focused architecture/editor tests,
  and `UnitTests` passed during Phase 6D verification.

Phase 6D proves:

- `IPanel` + pimpl is viable for at least one simple panel.
- Public Qt exposure can be reduced without breaking the editor build.
- The Phase 6B guard can enforce allowlist reduction.
- Migration can proceed panel by panel.

Phase 6D does not prove:

- `ProfilerPanel`, `ProjectBrowserPanel`, or `CollaborationPanel` are migrated.
- Visual scripting public headers are clean.
- `QtPanel` itself is removable.
- `SagaEditorModule` product ABI is clean.
- CMake Qt PUBLIC visibility can change.

## ProfilerPanel Inspection

`ProfilerPanel` was inspected before editing.

| Question | Result |
|---|---|
| Public header Qt exposure | `QtPanel` include/inheritance only |
| Meaningful implementation logic | No; the source file was a stub |
| Profiler data/model dependency | None in the editor panel source |
| EditorShell registration assumption | Standard `IPanel` registration through stable panel id |
| QWidget/QObject behavior beyond native widget | None found |
| Existing focused panel test | None |
| Migration safety | Safe to use the `GraphViewportPanel` pattern |

The stable panel id remains `saga.panel.profiler`, matching existing
persona/profile references.

Rollback:

- restore `ProfilerPanel.h` to inherit `QtPanel`
- remove `Editor/src/SagaEditor/UI/Qt/Panels/ProfilerPanel.cpp`
- restore the `ProfilerPanel.h` allowlist entry
- remove this checkpoint and docs references

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/Panels/ProfilerPanel.h` no longer includes
  `SagaEditor/UI/Qt/QtPanel.h`.
- `ProfilerPanel` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public class name and default constructor are preserved.
- The header now exposes the existing backend-neutral native handle method:
  `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/Panels/ProfilerPanel.cpp` owns the private Qt
  widget through `ProfilerPanel::Impl`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.profiler`.
- `GetTitle()` returns `Profiler`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The old `Editor/src/SagaEditor/Panels/ProfilerPanel.cpp` is a non-defining
  moved stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/Panels/ProfilerPanel.h` from the allowlist.
- All other Phase 6B allowlisted leaks remain unchanged.
- `QtPanel.h` remains temporary allowlisted debt.

## Remaining Public Qt Leaks

Still allowlisted after Phase 6E:

- `Editor/include/SagaEditor/UI/Qt/QtPanel.h`
- `Editor/include/SagaEditor/Panels/CollaborationPanel.h`
- `Editor/include/SagaEditor/Panels/ProjectBrowserPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h`
- `Editor/include/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/WatchPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/BreakpointPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/NodePalette.h`
- `Editor/include/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.h`
- `Apps/Saga/SagaEditorModule.h`

## Non-Goals

Phase 6E does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate more than one panel
- migrate `ProjectBrowserPanel`
- migrate `CollaborationPanel`
- migrate visual scripting UI
- change `SagaEditorModule`
- rewrite `EditorShell`
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
- targeted `rg` for `ProfilerPanel`, `GraphViewportPanel`, `QtPanel`, public
  Qt exposure, architecture allowlist, Phase 6E, `IPanel`, and pimpl wording
- CTest label inventory for `architecture`, `editor`, `ui`, `product`,
  `collaboration`, and `integration`

`EditorQtPublicAbiBoundaryTests` reported 288 scanned public headers, 41
allowed legacy Qt exposure findings, and 0 violations.

No focused behavior test was added. Constructing the migrated panel creates a
Qt widget and the current safe unit seams do not provide a narrow
QApplication-backed panel construction target.

Recommended Phase 6F: migrate `ProjectBrowserPanel` with the same `IPanel` +
pimpl/private Qt widget pattern if Phase 6E verification stays clean.
