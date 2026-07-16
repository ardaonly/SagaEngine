---
title: Repository map
status: current
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Repository map

The hard cutover made ownership visible in the filesystem. New code should strengthen this layout rather than rebuild compatibility paths around the old one.

| Area | Responsibility |
| --- | --- |
| `Engine/Source/Runtime/<Module>` | Runtime contracts and implementation, separated into `Public`, `Private`, and `Tests`. |
| `Engine/Source/Editor/<Module>` | Editor contracts and implementation with the same public/private ownership shape. |
| `Engine/Source/Programs/<Program>` | Thin entry points and program-specific assembly. |
| `Tools/<PermanentTool>` | Maintained project, script, packaging, and developer-facing tools. |
| `Tools/Developer/<Check>` | Internal repository and architecture checks. |
| `Tests/<EvidenceArea>` | Cross-cutting unit, integration, GPU, contract, and evidence suites. |
| `Samples` | Sample-owned projects, content, and source. |
| `SagaWiki` | Current human-facing knowledge and its offline reader. |

## Runtime modules

The runtime currently includes Core, Math, Diagnostics, ECS, Assets, Resources, Input, Audio, Render, RHI, UI, Scripting, Networking, Replication, Persistence, Simulation, World, ServerAuthority, and SagaRuntime.

## Editor modules

The editor currently includes EditorCore, EditorFramework, EditorQt, EditorAuthoring, VisualBlocksEditor, EditorCollaboration, and EditorExperimental. Runtime script lifecycle belongs to Runtime/Scripting; editor-facing script inspection and patch workflows belong to EditorAuthoring and VisualBlocksEditor.

## Programs

Programs are not architecture owners. SagaEditor, SagaEditorLab, SagaLauncher, SagaRuntime, and SagaSandbox assemble modules, but reusable behavior belongs in the owning runtime or editor module. There is no SagaServer program owner; authoritative foundations remain in Runtime/ServerAuthority and the `SagaServerLib` library target. The existence of a program directory or library target does not prove a complete application workflow. See [Programs and local workflows](programs-and-workflows.md) for their bounded roles.

## Retired roots

The root-level `Runtime/`, `Server/`, `Backends/`, `Apps/`, `Shared/`, `Collaboration/`, and `Tools/scripts/` layouts are retired. Historical references can remain in Git history, but current includes, build rules, and documentation should name the owning post-cutover path.

For the complete owner/dependency review rules, see [Ownership and dependency contracts](../reference/ownership-and-dependency-contracts.md).
