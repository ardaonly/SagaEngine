# Saga Editor Customization Model

Phase 23 status is `Implemented-Unverified`.

Phase 23 adds a narrow report-level customization proof to the existing
`SagaEditor --inspect-project` mode. It records selected profile/view preset
metadata, panel/workflow visibility metadata, and read-only capability flags.
It does not change live editor UI behavior or project truth.

## Current Surfaces

The current editor already has built-in shell profiles:

- `saga.profile.basic`
- `saga.profile.node_editor`
- `saga.profile.standard_pipeline`
- `saga.profile.advanced_pipeline`
- `saga.profile.custom`

Those profiles carry layout preset ids, shortcut map ids, default panels,
toolbar commands, visible tool commands, and exposure flags for graph editor,
profiler, console, and asset browser surfaces.

`EditorShell` also supports panel registration, profile commands, profile layout
application, and customization panel commands. Phase 23 does not require a
compiled editor composition snapshot and does not apply profile changes to a
running UI.

Read-only authoring views remain the report source:

- `TechnicalPreviewProjectView`
- `ProjectBrowserWorkflowView`
- diagnostics panel read models
- script projection and inspection read models

## Report Presets

`SagaEditor --inspect-project ... --editor-shell-report ...` uses the existing
`--profile <id>` argument as the report-level selector. If omitted, the report
uses `technical_preview`.

Report aliases map to existing built-in profiles:

| Report preset | Built-in profile |
|---|---|
| `technical_preview` | `saga.profile.basic` |
| `script_authoring` | `saga.profile.node_editor` |
| `diagnostics` | `saga.profile.advanced_pipeline` |
| `server_authority` | `saga.profile.advanced_pipeline` |

Unknown aliases or profile ids do not fail report generation. They set
`customization.status` to `UnknownProfile`, include a diagnostic, and fall back
to `technical_preview` metadata.

## Report Fields

The report adds a `customization` object with:

- `status`, `requestedProfileId`, `resolvedProfileId`, and `viewPresetId`
- `displayName`, `layoutPresetId`, and `shortcutMapId`
- `personalPreferenceOnly: true`
- `sharedProjectTruthMutable: false`
- `profileSource: "builtin-report-metadata"`
- `visiblePanels`
- `workflowVisibility`
- read-only `capabilities`

Capability flags intentionally deny project mutation, in-place C# editing,
Visual Blocks editor UI, collaboration server configuration, and distribution
build support.

## Visibility Rules

No preset hides failures or removes known limitations.

- `technical_preview` makes project, runtime smoke, server smoke, package
  preflight, and CLI-only script projection references primary.
- `script_authoring` makes project, SagaScript analyze/compile, and CLI-only
  block evidence primary. Runtime, server, and package references remain
  available but secondary.
- `diagnostics` makes project validation, runtime smoke, and server smoke
  report references primary. Script, block, and package references remain
  available but secondary.
- `server_authority` makes project and server smoke report references primary.
  Runtime, script, block, and package references remain available but secondary.

## Shared Truth Boundary

Customization metadata is personal editor/report preference only. It must not
mutate `.sagaproj` files, scenes, scripts, reports, package profiles, local
overlays, or `.sde` files. Shared project truth remains owned by project files
and existing CLI/tool reports.

## Non-Claims

Phase 23 does not implement or claim:

- maximum customization;
- a full plugin system;
- scriptable editor UI;
- collaborative workspace customization;
- enterprise policy-aware UI;
- a custom editor DSL;
- real profile editing in the UI;
- live UI layout mutation from the no-UI inspection mode.

No phase is marked `Verified`.
