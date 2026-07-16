---
title: Editor shell and authoring contracts
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Editor shell and authoring contracts

This reference defines the durable editor shell, project inspection, authoring transaction, Qt, customization, and product-workflow boundaries. It describes current owners and honest integration direction; it does not present the current editor as a finished product.

## Module ownership

The post-cutover editor is divided deliberately:

- `EditorCore` owns editor-neutral commands, identifiers, base state, and low-level editor contracts.
- `EditorFramework` owns shell composition, panels, docking, layouts, workspaces, notifications, localization, profiles, personas, themes, customization, and host orchestration without owning Qt ABI.
- `EditorQt` owns Qt-specific application/window/widget and adapter implementation.
- `EditorAuthoring` owns project/scene/asset/script inspection and semantic authoring workflows, viewport controllers, gizmos, import/cook coordination, inspectors, prefabs, and world-editing contracts.
- `VisualBlocksEditor` publicly owns the evidenced `Blocks/**` authoring and lowering contracts; its read-only product descriptor remains private integration evidence.
- `Runtime/Scripting` owns script lifecycle and hosting; `EditorAuthoring` and `VisualBlocksEditor` own current editor-facing script inspection, projection, and patch workflows.
- `EditorCollaboration` owns the modern `SagaCollaboration` services/model and `SagaShared` collaboration/workspace values; the legacy `SagaEditor::Collaboration` surface is retired.
- `EditorExperimental` owns deliberately unstable ideas that should not be advertised as stable editor contract.

The `SagaEditor` program composes these owners. It does not absorb their reusable behavior. `SagaEditorLab` is a development/evidence host and is not the production editor identity.

## Shell role

The shell is an interaction and orchestration layer. It can open a project, present validation and diagnostics, route commands, host panels, maintain view state, request authoring transactions, launch bounded runtime/script/package workflows, and display their reports. It does not replace SagaProjectKit, SagaScript, SagaPackager, runtime modules, or authoritative source files.

A tool failure remains a failed workflow step. The shell preserves the command or operation identity, exit/terminal state, report path or payload, and diagnostics. It must not show a green “done” state because a process launched while ignoring a nonzero exit or missing report.

## Minimum workflow

The durable editor/product flow is:

1. Select or open a `.sagaproj` file.
2. Validate the manifest and project boundary.
3. Build an inspection/readiness view from validator and artifact reports.
4. Navigate project, scene, asset, script, and diagnostic views.
5. Form a semantic edit/operation against an identified source version.
6. Preview and validate the operation.
7. Apply through the owning authoring tool/transaction when supported.
8. Revalidate affected source and derived artifacts.
9. Launch bounded play/runtime, scripting, or package workflows if requested.
10. Surface actual output, failure, and known limitation.

Not every step currently has a complete visual UI. A model/view or CLI evidence path is described as such.

## Project open

Project open begins with a path selected by the user or launcher. The shell normalizes it, invokes the current validator, and receives structured identity, schema, path, source, slice, and artifact diagnostics. It does not walk upward/downward looking for another project after the selected manifest fails.

An open session records project root, manifest path, project identity, schema version, active profile/slice, and validation generation. A later report or transaction is checked against that identity/generation so output from one project cannot be displayed as another project's state.

The editor can support inspection of an invalid project, but that mode is explicit and read-only where source authority cannot be established. It does not describe the project as ready to run or publish.

## Readiness and diagnostics views

`ProjectReadinessView`, project-browser workflow values, diagnostic panel groups, scene/asset inventory, script artifact index, and related authoring views are copied presentation models. They summarize tool/module outputs for UI use. They are not new authorities.

A readiness view should distinguish:

- valid current source;
- missing required source or artifact;
- malformed/unsupported schema;
- stale generated artifact;
- optional unavailable capability;
- blocked runtime/package action;
- known unimplemented workflow.

Diagnostics group by owner, severity, stable id, project object, and workflow step. Filters never delete the underlying report. A “no visible problems” filtered view is not the same as a passing project.

## Shared project model

Shared project state includes canonical project/scene/entity/script/asset source, versioned metadata deliberately stored with the project, and accepted semantic transactions. It follows schema and source-control policy.

The editor works against semantic object identity rather than widget identity. A scene entity property edit targets project, scene, entity, component, property, expected source version, and new typed value. A script patch targets file, block/construct, exact span, source hash, and operation. This makes the change reviewable independent of which panel initiated it.

Shared state is not automatically collaborative or remote. It can be local files and local metadata with transaction discipline.

## Personal view state

Personal state includes panel visibility, dock arrangement, window geometry, local selection, focus, recent navigation, density/theme, shortcuts, and personal workspace presets unless a policy explicitly promotes a field into project source.

Personal state must not alter build/runtime behavior. A hidden diagnostics panel does not suppress diagnostics. Hiding a project slice in a personal view does not remove it from the manifest. A local shortcut override does not change a shared semantic command id.

Personal overlays should be stored outside canonical project source or in a clearly personal scope. Applying an overlay validates referenced panels/actions and reports unknown or conflicting entries.

## Editor composition

Editor composition artifacts describe panels, actions, menus, toolbars, shortcuts, workspace layouts/profiles, and modes using editor-owned values. A resolver combines the base composition with supported overlays and yields a resolved immutable snapshot. A diff explains what an overlay changes.

Resolution is deterministic. Duplicate ids, missing action references, invalid menu/toolbar entries, shortcut conflicts, unknown panels, and unsupported capabilities produce diagnostics. The resolver does not instantiate Qt widgets or execute arbitrary code while merely reading composition metadata.

Composition artifacts are derived/configuration inputs according to their contract. A generated composition snapshot is not authoritative source and is rebuilt when its base or overlay changes.

## Panel contract

An editor-neutral panel has identity, title/role, lifecycle, visibility, and model/update behavior independent of Qt. Panels request commands or semantic operations through services. They do not reach through the shell to mutate unrelated managers.

Project browser, hierarchy, inspector, world viewport, graph viewport, problems, console, profiler, collaboration, customization, and dashboard-like panels are capability surfaces. A class existing under `Public` does not prove complete UI wiring or production-ready behavior. The owning view/model and tests determine the claim.

## Qt boundary

Qt is permitted in `EditorQt` public/implementation surfaces where Qt ABI is the intended contract. It must not leak into `EditorCore`, `EditorFramework`, `EditorAuthoring`, Visual Blocks data contracts, runtime modules, or installed vendor-neutral headers.

Editor-neutral code uses plain values, interfaces, callbacks, command ids, and snapshots. EditorQt translates those into `QObject`, widgets, models, signals/slots, actions, and windows. This direction makes headless tests and alternate hosts possible without pretending an alternate UI toolkit exists.

Widget lifetime follows Qt ownership inside EditorQt. Domain/model lifetime remains with the editor service/session that owns it. A widget pointer is not stored as canonical project state.

## Commands and semantic operations

A shell command represents user intent such as open project, validate, save supported changes, run a workflow, show a panel, or request a transaction. Commands have stable ids and enabled/disabled reasons. A disabled action should explain the missing capability, invalid state, or permission decision.

Semantic operations carry typed target and expected version. Preview calculates proposed changes and diagnostics without mutation. Apply verifies freshness and policy again, writes through the owner, and emits a result. UI buttons never bypass this path to edit files directly.

Undo/redo is operation-based where a reversible transaction exists. It is not guaranteed for external tools, runtime launches, package writes, or arbitrary source rewrites. The UI states the boundary rather than adding a fake undo item.

## Inspector editing

Inspector property descriptors identify type, display metadata, constraints, current value, and target/version context. A property editor backend provides UI-neutral get/validate/submit behavior. EditorQt supplies controls; the binder translates validated user input into a semantic change request.

Changing a text box is not a committed project edit. Parse/type/range validation occurs first. The source/model owner accepts or rejects the transaction. If the source changed after the inspector view was built, apply is stale and the view refreshes or offers a deliberate merge/retry.

Custom property editors register by stable type/capability. Registration conflicts and missing editors are diagnosed. A custom widget does not gain direct access to private scene/ECS storage.

## Scene and viewport editing

Scene hierarchy and viewport controllers present a validated authoring model. Selection, camera motion, gizmo hover, and viewport navigation are personal/transient view state. Transform or component edits become semantic operations against scene/entity source.

Gizmos compute candidate transforms using the canonical coordinate convention. Snapping and local/world modes are explicit inputs. Commit validates entity/component identity and source freshness. Cancelling a drag restores the pre-operation model without writing source.

Preview runtime/ECS objects are derived from the authoring model. They can be rebuilt after source change. Editing a preview object directly without a source transaction is not durable authoring.

## Asset and content authoring

Asset evaluation/import views inspect source and proposed output. Import/cook owners validate format, settings, destination/project boundary, identity, dependencies, and generated artifact provenance. The editor displays progress and diagnostics; it does not reinterpret a failed import as an empty successful asset.

The scene/asset browser inventory lists canonical source and known artifact state. It distinguishes missing, stale, invalid, unsupported, and ready. It does not glob arbitrary repository files into project truth.

Derived-data cache entries are disposable and keyed by source/settings/tool contract. Cache presence never replaces source validation.

## Script authoring integration

The editor can show script behavior inspection/projection, compatibility, patch preview, review, and artifact freshness views. C# remains authoritative. SagaScript or the scripting owner performs analysis, compilation, projection, and supported patch application. Editor panels consume versioned artifacts/reports and request operations.

Visual Blocks are not executed as a second runtime graph. The editor must not silently rewrite a whole C# file to make the visual view convenient. See the detailed C#/Visual Blocks reference.

## Customization model

Customization can cover personal layout, panel visibility, shortcuts, toolbar visibility/order, theme/density/persona, and supported workspace profiles. Each capability is advertised explicitly by the current catalog. Unsupported customization remains unavailable rather than stored and ignored.

Resolution order is deliberate: stable base composition, selected editor/project profile where valid, then personal overlay. Shared project truth is not overridden by a personal UI preference. Team-shared layout would require its own schema and authority; it is not implied by the presence of workspace types.

Shortcut changes bind to semantic action ids. Conflicts, reserved shortcuts, unknown actions, and duplicate bindings produce feedback. The session can preview before storing the personal override.

## Product and launcher workflow

`SagaLauncher` or an editor start surface can route users to validation, editor, runtime, scripting, and package tools. It preserves owner and report identity. The shell is not itself a compiler, package builder, runtime engine, or server.

A workflow record includes project/profile, step id, invoked owner/command, start/end/exit state, report path or structured result, diagnostics, and non-claims. Reference-only steps are labeled not executed. A missing report is a failure/limitation, not success.

Runtime play can be a bounded smoke or interactive host depending on the actual command. Script/blocks can be a CLI/test-level chain. Server evidence can be ServerAuthority library smoke. Package preflight can fail honestly when inputs are missing. UI wording preserves those distinctions.

## Error and cancellation behavior

Long operations expose pending/progress/cancelled/failed/completed state. Cancellation is requested through the owner; the shell does not destroy a worker thread. Late results carry session/generation and are discarded when the project or view changed.

Closing a project stops new authoring operations, asks active tools/services to quiesce, releases preview/runtime resources, and then clears view state. Unsaved supported transactions are surfaced. The editor does not write every open model automatically during shutdown.

## Evidence boundary

Model/unit tests prove resolver, overlay, command, view, transaction, stale, and diagnostic behavior. Qt tests prove adapter/widget ABI and interaction in that host. Tool integration tests prove report routing. Runtime or package tests prove their own workflows. A screenshot or the existence of panel classes does not prove a finished editor.

## Non-claims

The current surface does not establish a finished editor, stable public editor SDK, arbitrary plugin ecosystem, complete scene/prefab/world authoring, safe undo for every operation, hosted collaboration, maximum customization, or production packaging. Public manager/widget/host types can be transitional and require later visibility audit.

## Change checklist

- Put editor-neutral behavior below EditorQt.
- Keep program entry and shell routing thin.
- Preserve the owner/tool result and failure in every workflow.
- Separate shared project truth from personal view state.
- Express source changes as freshness-checked semantic operations.
- Keep previews and runtime objects derived from source.
- Advertise only supported customization capabilities.
- Test model behavior separately from Qt interaction.
- Never use a visible panel or class name as the sole evidence of product completion.
