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
| `SagaRuntime` | Thin runtime application entry, command-line configuration, runtime host, and asset startup bootstrap. |
| `SagaEditor` | Qt-backed editor shell entry over editor modules. |
| `SagaEditorLab` | Development scenarios for editor host/shell, panels, profiles, customization, and deterministic checks. |
| `SagaLauncher` | Local project catalog/workspace and product-workflow router that surfaces tool/process results. |
| `SagaSandbox` | Manual and development scenarios for runtime subsystems, probes, rendering, and diagnostics. |
| `SagaServer` | Reserved program owner with no current executable implementation; ServerAuthority foundations live in runtime modules. |

## Product workflow routing

SagaLauncher may open or select a project, invoke the owning validator or tool, launch a program, and surface exit status, diagnostics, and report paths. It must not replace SagaProjectKit, SagaScript, SagaPackager, the editor, or the runtime with a second implementation. A workflow step is successful only when its owning operation produced valid evidence.

## Development programs

EditorLab and Sandbox are development/evidence surfaces. Deterministic subsystem assertions should move into focused automated tests when practical; interactive scenarios remain useful where observation or device behavior matters. Neither program defines public engine API merely because it can reach several modules.

## Placeholder honesty

`Engine/Source/Programs/SagaServer` currently contains build ownership scaffolding but no `main.cpp`. This is not a dedicated-server product or a network-service claim. Current authority behavior is documented under [Networking, replication, and authority](../runtime/networking-and-authority.md); any future server entry point should remain a thin launcher over those owners.

## Workflow non-claims

Local shell, report, and smoke paths do not establish a finished launcher, finished editor, production server, distribution readiness, or hosted collaboration. They make current boundaries inspectable and failures visible.

## Detailed references

- [Editor shell and authoring contracts](../reference/editor-shell-and-authoring.md)
- [Runtime lifecycle, assets, and packages](../reference/runtime-lifecycle-assets-and-packages.md)
- [Replication, networking, and server authority](../reference/replication-networking-and-authority.md)
