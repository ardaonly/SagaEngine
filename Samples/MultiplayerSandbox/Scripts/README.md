# Scripts

SagaScript/SagaWeaver fixtures for MultiplayerSandbox source-truth evidence.

These files are source-trust fixtures:

- `DoorLogic.High.cs` demonstrates `Gameplay + High` metadata.
- `DoorState.Low.cs` demonstrates `Gameplay + Low` metadata.
- `AdvancedUnsupported.cs` demonstrates unsupported C# projected as opaque,
  read-only source-linked nodes.

The C# files remain canonical source. SagaScript analysis, projection, source
maps, runtime binding metadata, and patch previews must not rewrite them.

This folder does not provide finished gameplay, editor node UI, live code
reload, debug tooling, runtime-backed C# blocks, or a hardened script sandbox.
