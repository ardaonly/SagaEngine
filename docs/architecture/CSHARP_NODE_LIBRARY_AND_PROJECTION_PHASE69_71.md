# C# Node Library and Projection Phase 69-71

Status: Hedef 2 Block B Phase 69-71 focused evidence.

This slice implements the metadata/projection-safe first batch of Block B. It
does not implement source-changing patch behavior, undo/redo, compatibility
profile v2, package/publish integration, editor UI, Qt UI, runtime gameplay,
server gameplay, ClientHost work, Phase 76+, or Hedef 3.

## Implemented Scope

Phase 69 adds `sagascript extract-nodes`, which reads C# source and emits
`node_library_report.json` from `[SagaLibrary]` and `[SagaNode]` metadata. The
report follows the shared artifact envelope and includes source files, stable
library ids, stable node ids, source spans, api domain, api level, capability,
compatibility, diagnostics, and deterministic status.

Phase 70 adds tool-local built-in Gameplay.High node fixtures:

- `Gameplay.High.Inventory.Has`
- `Gameplay.High.Door.Open`
- `Gameplay.High.Score.Add`
- `Gameplay.High.Audio.PlayEvent`
- `Gameplay.High.Entity.SetTag`

These nodes are `ProjectionOnly`. They are metadata/projection evidence only
and are not runtime behavior proof.

Phase 71 adds tool-local Gameplay.Low node fixtures:

- `Gameplay.Low.Trigger.OnEnter`
- `Gameplay.Low.Spawn.Entity`
- `Gameplay.Low.Timer.Delay`
- `Gameplay.Low.Entity.DestroyOrDeactivate`

These nodes are `Deferred`. They must be disclosed as read-only and must not be
treated as executable or editable runtime behavior.

## Projection Rules

`projection_report.json` and `node_metadata.json` now include capability and
projection compatibility fields. Supported values used in this slice are:

- `ProjectionOnly`
- `Deferred`
- `EditablePreviewOnly`
- `ReadOnly`
- `Opaque`

Deferred nodes are disclosed as read-only. Unsupported or advanced C# remains
opaque/read-only with source spans. Projection does not modify source files.

## Honesty Boundary

SagaViewKit Simple View honesty checks treat deferred and opaque/unsupported
nodes as advanced regions. Hidden deferred metadata or editable deferred nodes
fail the honesty check.

## Deferred

Phase 72-75 remain separate follow-up batches. This slice does not add a
source-changing command, rollback, undo/redo, compatibility profile v2, runtime
proof upgrades, or package/publish gate integration.

## Evidence

Focused evidence:

```sh
dotnet build Tools/SagaScript/SagaScript.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaScript/tests/test_sagascript_cli.py
dotnet build Tools/SagaViewKit/SagaViewKit.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaViewKit/tests/test_sagaviewkit_cli.py
```
