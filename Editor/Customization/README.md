# Safe Workspace Customization

SagaEditor workspace customization is user preference data layered on top of a
compiled editor composition snapshot.

Canonical editor structure is not stored in customization overlays. The source
of truth for editor structure is:

```txt
Editor/CompositionSources/source/*.sde
```

The product pipeline compiles that source through SagaPipeline and the
SagaEditorComposition tool into generated artifacts and manifests under
`Build/Artifacts`, `Build/Manifests`, and `Build/Reports`. Editor runtime code
loads documented compiled manifests and artifacts. It does not invoke Forge,
SDE, SagaPipeline, or the composition compiler at startup.

The safe customization flow is:

```txt
ResolvedEditorCompositionSnapshot
    -> EditorCustomizationCapabilityModel
    -> WorkspaceCustomizationModel
    -> WorkspaceCustomizationController
    -> EditorCustomizationOverlay delta
    -> WorkspaceCustomizationOverlayStore
```

`ResolvedEditorCompositionSnapshot` remains immutable. The controller edits only
safe overlay deltas. Overlays cannot define panels, actions, menus, toolbars,
workspaces, editor modes, service graph bindings, plugin classes, or internal
shell behavior.

`WorkspaceCustomizationSession` is the host-owned integration layer. It loads
the default user overlay, owns the controller, publishes customization
diagnostics to the editor diagnostics service, and exposes model/save/reset
operations for future UI. `EditorShell` reports registered panel ids back to the
host after panel registration so availability reflects real panel
implementations. Command/action availability comes from `CommandRegistry`.

## Product UI

The Safe Customize Workspace and Safe Shortcut Preferences UIs are product
editor surfaces over host-owned customization sessions:

```txt
EditorHost
    -> WorkspaceCustomizationSession
    -> WorkspaceCustomizationPanelViewModel
    -> CustomizeWorkspacePanel

EditorHost
    -> ShortcutCustomizationSession
    -> ShortcutCustomizationPanelViewModel
    -> ShortcutPreferencesPanel
```

The workspace panel edits only user overlay visibility deltas through the session. It does
not parse overlay JSON directly, decide whether panels are editable, mutate the
resolved composition snapshot, discover shell panels itself, edit `.sde` source,
or invoke Forge, SDE, SagaPipeline, or the composition compiler.

Locked, internal-only, or unavailable panels are rendered as non-editable rows
with a locked reason. Missing composition state produces an unconfigured view
state instead of failing editor startup.

The UI also supports importing and exporting workspace overlay files. Import is
restricted to workspace visibility/layout data that can be validated against the
current resolved composition snapshot. Shortcut and toolbar overrides are not
accepted through this panel because the workspace UI does not own those
customization surfaces.

The shortcut preferences panel edits only `shortcutOverrides` in the same safe
overlay schema. It derives action availability from the host command registry and
only exposes product-safe, implemented, shortcut-assignable actions as editable.
Unknown, internal-only, non-product-safe, unavailable, invalid chord, and
collision cases produce structured customization diagnostics instead of silent
fallbacks.

Workspace and shortcut panels share the same default overlay file. Each session
preserves overlay sections owned by the other session when saving or resetting,
so resetting workspace visibility does not delete shortcut preferences and
resetting shortcut preferences does not delete workspace visibility overrides.

Save, reset, reload, import, and export operations publish transient editor
notifications through `EditorNotificationCenter`. Those notifications are
user-facing status feedback only. Structured customization diagnostics remain
the source of detailed failure truth.
Notification formatting is kept in Qt-free customization code so save/reset/
import/export status text can be tested without launching the full Qt editor.

The default overlay path policy is:

```txt
<workspace root>/.saga/editor/customization/workspace.overlay.json
```

This file is user-private preference state. It is not a generated build artifact
and should not be treated as canonical editor structure.

## Apply Policy

This slice uses a restart-required policy.

The existing shell can project panel visibility from a resolved startup
snapshot, but customization changes do not yet have a public live-apply
contract. The controller therefore reports `restartRequired` when a safe overlay
delta is dirty. It does not pretend that shell state was updated live.

Save writes the safe overlay. Reset clears it. Both operations still leave
runtime shell state unchanged until the editor is restarted or a future tested
live-apply contract is added.

Future live apply can be added for narrow cases such as panel visibility once
the shell exposes a tested safe application path.

## Smoke Validation

The SagaEditor launcher supports a bounded smoke mode for automated startup QA:

```txt
SagaEditor --smoke --no-show --windowed --smoke-timeout-ms 1000
```

Smoke mode initializes the normal editor app, host, shell, and registered
product panels, including Customize Workspace, then runs a bounded UI event loop
and exits. It does not invoke SDE, Forge, SagaPipeline, or composition compiler
tools at runtime.

## Boundaries

- Customization model, controller, diagnostics, and store code are Qt-free.
- Customization core does not parse `.sde`.
- SagaEditor consumes compiled manifests and artifacts only.
- SagaPipeline owns product orchestration; editor runtime does not invoke it.
- Apps/SagaEditorComposer and internal mutation tools are deferred.
