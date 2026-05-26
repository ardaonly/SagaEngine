# Phase 6D First Panel pimpl Migration

> Last updated: 2026-05-25
> Status: Phase 6D bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6D closes Phase 6C honestly and migrates exactly one public panel header
away from `QtPanel` inheritance. The selected panel is `GraphViewportPanel`.

Full CTest remains unverified.

## Phase 6C Closure

Phase 6C is accepted as:

- QtPanel public ABI replacement plan
- `IPanel` + pimpl/private Qt implementation migration strategy
- migration order checkpoint

Phase 6C is not accepted as:

- QtPanel removal
- panel migration
- visual scripting migration
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup

Full CTest remains unverified.

Completion evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6C_QTPANEL_PUBLIC_ABI_REPLACEMENT_PLAN.md` exists.
- `docs/dev/ITERATION_NOTES.md` records the Phase 6C decision.
- `docs/roadmaps/ENGINE_RECOVERY_ROADMAP.md` records Phase 6C evidence.
- `IPanel` is selected as the replacement contract.
- The migration order is recorded.
- No optional public API was added in Phase 6C.
- The Phase 6B guard regression passed during Phase 6C verification.

Phase 6C proves:

- the `QtPanel` root cause is understood
- the replacement design is selected
- the first migration direction is defined
- broad Qt removal is intentionally deferred

Phase 6C does not prove:

- public Editor headers are Qt-free
- any allowlist entry was removed
- Qt dependency visibility can change
- runtime or editor behavior changed

## Candidate Decision

`GraphViewportPanel` and `ProfilerPanel` were both inspected before choosing
the first migration target.

| Candidate | Public Qt exposure | Current inheritance | Public surface | Dependencies | Implementation complexity | EditorShell impact | Tests available | Expected allowlist change | Risk | Decision |
|---|---|---|---|---|---|---|---|---|---|---|
| `GraphViewportPanel` | `QtPanel` include/inheritance | `public QtPanel` | default constructor, `GetPanelId`, `GetTitle`, `OnInit` | Persona/profile references to `saga.panel.graph`; no source implementation coupling found | Low; current `.cpp` is a stub | None; still registers as `IPanel` | Build, architecture guard, existing editor shell seams | remove `GraphViewportPanel.h` only | Low-medium | Chosen |
| `ProfilerPanel` | `QtPanel` include/inheritance | `public QtPanel` | default constructor, `GetPanelId`, `GetTitle`, `OnInit` | Persona/profile references to `saga.panel.profiler`; no source implementation coupling found | Low; current `.cpp` is a stub | None | Build, architecture guard | would remove `ProfilerPanel.h` only | Low-medium | Defer to Phase 6E |

`GraphViewportPanel` was chosen because it matches the Phase 6C default and no
additional rendering/view-model coupling was present in the current source.
`ProfilerPanel` remains the next simple migration candidate.

Rollback:

- restore `GraphViewportPanel.h` to inherit `QtPanel`
- remove `Editor/src/SagaEditor/UI/Qt/Panels/GraphViewportPanel.cpp`
- restore the `GraphViewportPanel.h` allowlist entry
- remove this checkpoint and docs references

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/Panels/GraphViewportPanel.h` no longer includes
  `SagaEditor/UI/Qt/QtPanel.h`.
- `GraphViewportPanel` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public class name and default constructor are preserved.
- The header now exposes the existing backend-neutral native handle method:
  `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/Panels/GraphViewportPanel.cpp` owns the private
  Qt widget through `GraphViewportPanel::Impl`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.graph`, matching existing persona/profile
  references.
- `GetTitle()` returns `Graph`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The old `Editor/src/SagaEditor/Panels/GraphViewportPanel.cpp` is a
  non-defining forwarding stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/Panels/GraphViewportPanel.h` from the allowlist.
- All other Phase 6B allowlisted leaks remain unchanged.
- `QtPanel.h` remains temporary allowlisted debt.

## Remaining Public Qt Leaks

Still allowlisted after Phase 6D:

- `Editor/include/SagaEditor/UI/Qt/QtPanel.h`
- `Editor/include/SagaEditor/Panels/ProfilerPanel.h`
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

Phase 6D does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate more than one panel
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

Required verification:

- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`
- `git diff --check`
- targeted `rg` for selected panel, `QtPanel`, public Qt exposure,
  architecture allowlist, Phase 6D, `IPanel`, and pimpl wording
- CTest label inventory for `architecture`, `editor`, `ui`, `product`,
  `collaboration`, and `integration`

No focused behavior test was added. Constructing the migrated panel creates a
Qt widget and the current safe unit seams do not provide a narrow
QApplication-backed panel construction target.

Recommended Phase 6E: migrate `ProfilerPanel` with the same `IPanel` +
pimpl/private Qt widget pattern, unless Phase 6D verification exposes missing
panel semantics or a test seam that should be addressed first.
