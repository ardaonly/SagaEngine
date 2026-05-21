# SagaScript Toolchain

SagaScript is the standalone C# metadata validation and manifest emission tool
for the SagaScript ecosystem.

Current scope:

- Parse C# source with Roslyn.
- Validate SagaScript attributes and conservative v1 type support.
- Inspect block-callable bindings.
- Emit JSON-compatible binding, capability, artifact, and diagnostics reports.
- Compile validated script source into .NET 10 script assemblies.

Current non-goals:

- No CoreCLR host.
- No hostfxr/nethost integration.
- No runtime or server execution.
- No editor UI.
- No Forge or Prism implementation ownership.

Example:

```sh
Tools/SagaScript/sagascript validate --source Scripts --json
Tools/SagaScript/sagascript inspect-bindings --source Scripts --out Build/Manifests/script_bindings.inspect.json
Tools/SagaScript/sagascript emit-manifests --source Scripts --out Build/Manifests
Tools/SagaScript/sagascript compile --source Scripts --out Build/Manifests --artifacts-out Build/Artifacts/Scripts --project-root .
```

`compile` requires a .NET 10 SDK. When the SDK is missing, the launcher emits a
structured `Script.Toolchain.DotNetSdkMissing` diagnostic instead of allowing the
.NET SDK to print a raw target-framework error. SagaScript does not downgrade the
target framework automatically.

Compile outputs:

- `Build/Artifacts/Scripts/SagaProjectScripts.scripts.dll`
- `Build/Artifacts/Scripts/SagaProjectScripts.scripts.runtimeconfig.json`
- `Build/Artifacts/Scripts/SagaProjectScripts.scripts.pdb`
- `Build/Manifests/script_bindings.json`
- `Build/Manifests/script_capabilities.json`
- `Build/Manifests/script_artifacts.json`
- `Build/Manifests/artifact_manifest.scripts.json`
- `Build/Manifests/sagascript_diagnostics.json`
