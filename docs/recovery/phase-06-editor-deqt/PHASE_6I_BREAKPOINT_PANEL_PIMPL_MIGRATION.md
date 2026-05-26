# Phase 6I BreakpointPanel pimpl Migration

> Last updated: 2026-05-25
> Status: Phase 6I bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6I closes Phase 6H honestly and migrates exactly one additional visual
scripting leaf panel. The selected panel is `BreakpointPanel`.

Full CTest remains unverified.

## Phase 6H Closure

Phase 6H is accepted as:

- first visual scripting leaf pimpl migration
- `WatchPanel` public Qt exposure removal
- visual scripting cluster inventory
- Phase 6B allowlist reduction

Phase 6H is not accepted as:

- broad visual scripting migration
- migration of `BreakpointPanel`, `ExecutionTraceView`, `GraphDebuggerView`,
  `NodePalette`, `NodeLibraryPanel`, or `GraphCanvas`
- `QtPanel` removal
- `SagaEditorModule` de-Qtification
- CMake Qt PUBLIC dependency cleanup

Completion evidence:

- `WatchPanel` public header is Qt-free.
- `WatchPanel` now inherits `IPanel`.
- Its private Qt implementation exists under
  `Editor/src/SagaEditor/UI/Qt/VisualScripting/Editor`.
- The `WatchPanel.h` allowlist entry was removed.
- The visual scripting cluster inventory is recorded in
  `docs/recovery/phase-06-editor-deqt/PHASE_6H_VISUAL_SCRIPTING_QTPANEL_CLUSTER.md`.
- The boundary guard reported 288 scanned public headers, 32 allowed legacy Qt
  exposure findings, and 0 violations.
- Label inventory reported architecture 2, editor 2, ui 0, product 2,
  collaboration 0, and integration 3.

Phase 6H does not prove:

- the full visual scripting cluster is migrated
- debugger, trace, node palette, node library, or graph canvas ownership is
  clean
- `QtPanel` itself is removable
- `SagaEditorModule` product ABI is clean
- CMake Qt PUBLIC visibility can change

## BreakpointPanel Selection

`BreakpointPanel` is safe for this second visual scripting leaf migration
because it is still a panel shell:

- public header only exposed Qt through `QtPanel` include/inheritance
- default constructor only
- no public Qt types beyond `QtPanel`
- stub implementation
- no `BreakpointController`, debug session, graph runtime, command routing, or
  breakpoint storage ownership in the current source

The migration intentionally does not introduce debugger behavior.

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/VisualScripting/Editor/BreakpointPanel.h` no
  longer includes `SagaEditor/UI/Qt/QtPanel.h`.
- `BreakpointPanel` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public class name and default constructor are preserved.
- The header now exposes `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/VisualScripting/Editor/BreakpointPanel.cpp`
  owns the private Qt widget through `BreakpointPanel::Impl`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.visual_scripting.breakpoints`.
- `GetTitle()` returns `Breakpoints`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The placeholder label text is `Breakpoints`.
- The old `Editor/src/SagaEditor/VisualScripting/Editor/BreakpointPanel.cpp`
  is a non-defining moved stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/VisualScripting/Editor/BreakpointPanel.h` from the
  allowlist.
- All higher-risk visual scripting public Qt leaks remain allowlisted.
- `QtPanel.h` remains temporary allowlisted debt.

## Cluster Status After Phase 6I

- `WatchPanel`: migrated in Phase 6H
- `BreakpointPanel`: migrated in Phase 6I
- `ExecutionTraceView`: debugger/trace ownership audit required
- `GraphDebuggerView`: graph/debugger ownership audit required
- `NodePalette`: node/model ownership audit required
- `NodeLibraryPanel`: `NodeCategoryBrowser` boundary audit required
- `GraphCanvas`: high-risk `GraphDocument` view debt; defer
- `QtPanel.h`: remains public allowlisted debt
- `SagaEditorModule.h`: remains product ABI debt

## Non-Goals

Phase 6I does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate the full visual scripting cluster
- migrate `ExecutionTraceView`, `GraphDebuggerView`, `NodePalette`,
  `NodeLibraryPanel`, or `GraphCanvas`
- change `SagaEditorModule`
- rewrite `EditorShell`
- redesign visual scripting graph document, graph runtime, debugging, node
  library, or editor shell ownership
- wire `BreakpointController`
- add debugger session, breakpoint storage, command routing, or graph runtime
  behavior
- change `SagaEditorLib` Qt dependency visibility
- change runtime/client asset work
- change publish gates
- change collaboration or MultiplayerSandbox work
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `BreakpointPanel`, `WatchPanel`, remaining visual
  scripting headers, `QtPanel`, `EditorQtPublicAbiBoundary`, allowlist,
  `IPanel`, pimpl, and Phase 6I wording
- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R '^UnitTests$' --output-on-failure -j 1`
- CTest label inventory for `architecture`, `editor`, `ui`, `product`,
  `collaboration`, and `integration`

`EditorQtPublicAbiBoundaryTests` reported 288 scanned public headers, 29
allowed legacy Qt exposure findings, and 0 violations.

Label inventory reported architecture 2, editor 2, ui 0, product 2,
collaboration 0, and integration 3.

No focused behavior test was added. Constructing the migrated panel creates a
Qt widget and the current safe unit seams do not provide a narrow
QApplication-backed panel construction target.

Recommended Phase 6J: perform an `ExecutionTraceView` ownership audit rather
than immediate migration, because trace/debugger semantics are higher risk than
leaf shell panels.
