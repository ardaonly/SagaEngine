# SagaWeaver / SagaScript MVP Phase 34-43

> Block F evidence for Target 1 / Technical Preview Technical Preview product work.

Block F extends the existing `Tools/SagaScript` CLI instead of adding a second
tool. The implementation is source-preserving: user-authored C# remains the
canonical artifact and all block data is a projection over exact source spans.

## Two-Axis Public API Model

SagaScript classifies script-facing APIs with two explicit metadata axes.

The domain axis is:

- `Gameplay`
- `Runtime`
- `Server`
- `Networking`
- `UI`
- `Diagnostics`
- `Assets`
- `Packaging`

The abstraction axis is:

- `High`
- `Low`

The axes are independent. Low-level does not mean only server or runtime, and
high-level does not mean only designer gameplay. For Block F, the implemented
projection proof is intentionally narrow:

- `Gameplay + High`
- `Gameplay + Low`

`Server + Low` and `Diagnostics + Low` are schema-compatible future expansion
points unless backed by focused tests. Other domains are future-only in this
block.

## Metadata

The analyzer reads explicit metadata:

```csharp
[SagaBehavior(
    Id = "behavior://sample/door",
    Level = SagaApiLevel.High,
    Domain = SagaApiDomain.Gameplay)]
```

It also validates namespace usage such as `Saga.Gameplay.HighLevel` and
`Saga.Gameplay.LowLevel`. Mixed level or domain usage produces diagnostics
instead of inferred behavior.

Supported metadata contracts:

- `SagaApiLevel`
- `SagaApiDomain`
- `SagaBehavior`
- `SagaNode`
- `SagaLibrary`

## Artifacts

SagaScript writes Block F artifacts under `Build/SagaScript`:

- `analysis_report.json`
- `sagascript_diagnostics.json`
- `runtime_bindings.json`
- `source_map.json`
- `projection_report.json`
- `node_metadata.json`
- `patch_preview.json` when requested

`analysis_report.json`, `runtime_bindings.json`, `source_map.json`,
`projection_report.json`, and `node_metadata.json` preserve both `apiLevel` and
`apiDomain` data.

## Source Trust

The no-edit flow must preserve source bytes exactly. The tool does not format,
reorder using statements, delete comments, rewrite methods, regenerate classes,
or silently change unsupported regions.

Unsupported C# is projected as opaque, read-only nodes with source links.
`patch-preview` only produces a JSON preview for `ReplaceStringLiteral`; it does
not apply the patch or write C# source.

## MultiplayerSandbox Fixtures

`samples/MultiplayerSandbox/Scripts` contains:

- `DoorLogic.High.cs` for `Gameplay + High`
- `DoorState.Low.cs` for `Gameplay + Low`
- `AdvancedUnsupported.cs` for opaque/read-only projection evidence

These files are product proof fixtures, not finished gameplay.

## Non-Claims

Block F does not provide complete visual node authoring, general C# conversion,
a broad public engine API, live code reload, debug tooling, graph execution at
runtime, editor block UI, a hardened script sandbox, beta product status, or a
release-candidate claim.
