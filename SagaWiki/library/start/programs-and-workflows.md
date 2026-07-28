---
title: Programs and local workflows
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Programs and local workflows

Programs assemble owned modules into local entry points. They can coordinate validation, launch, inspection, and evidence, but reusable engine/editor behavior belongs in the relevant module.

| Program | Current bounded role |
| --- | --- |
| `Saga` | Permanent Qt desktop product: Home, personal Level 1/2/3 Authoring Mode selection, and the typed in-process Editor Workspace. It opens canonical projects and orchestrates verified Play without becoming a source/tool authority. |
| `SagaRuntime` | Thin runtime application entry and artifact-only Play host. It validates a canonical launch manifest/local envelope and consumes cooked scene, input, script, package, bridge, and render artifacts without parsing project source. |
| `SagaEditorLab` | Development scenarios for editor host/shell, panels, authoring modes, customization, and deterministic checks. |
| `SagaSandbox` | Manual and development scenarios for runtime subsystems, probes, rendering, and diagnostics. |

## Product workflow routing

Saga Home may open or select a project, invoke the owning validator or tool, and surface diagnostics. The in-process Editor Workspace may launch verified Play in `SagaRuntime`. Neither surface replaces SagaProjectKit, SagaScript, SagaPackager, the editor domain owners, or the runtime with a second implementation. A workflow step is successful only when its owning operation produced valid evidence.

The launcher does not orchestrate a sample-specific first-playable workflow. The
Editor Play path uses a generic deterministic runtime launch manifest and a
machine-local contained envelope; SagaRuntime rejects source files and unverified
or incompatible artifacts before its frame loop. Current interactive evidence is
bounded to StarterArena and is not a production launcher or arbitrary-project
compatibility claim.

The selected project contract is exclusively a `.sagaproj` manifest with `schemaVersion: 0`; legacy manifest filenames and unknown schema versions are rejected. Launcher has no duplicate package staging or publish-check implementation. Canonical package commands are `sagapack stage` and `sagapack publish-check`, whose results Launcher may only route or display.

## Development programs

EditorLab and Sandbox are development/evidence surfaces. Deterministic subsystem assertions should move into focused automated tests when practical; interactive scenarios remain useful where observation or device behavior matters. Neither program defines public engine API merely because it can reach several modules.

## Server entry-point boundary

There is no SagaServer program owner or dedicated-server entry point. `Engine/Source/Runtime/ServerAuthority` and the `SagaServerLib` library target contain authority foundations, not a deployed network service. Current authority behavior is documented under [Networking, replication, and authority](../runtime/networking-and-authority.md); any future server entry point should remain a thin launcher over those owners.

## Workflow non-claims

Local shell, report, and smoke paths do not establish a finished editor, production server, distribution readiness, or hosted collaboration. They make current boundaries inspectable and failures visible.

## Detailed references

- [Editor shell and authoring contracts](../reference/editor-shell-and-authoring.md)
- [Runtime lifecycle, assets, and packages](../reference/runtime-lifecycle-assets-and-packages.md)
- [Replication, networking, and server authority](../reference/replication-networking-and-authority.md)
