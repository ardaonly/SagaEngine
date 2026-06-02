# MultiplayerSandbox Project Fixture

`MultiplayerSandbox` is the canonical Technical Preview project truth fixture.
It records fixture evidence that a `.sagaproj` project can be validated,
resolved, diagnosed, and run through the bounded server-only headless preview
path.

This is not playable gameplay. The package profile declares server/headless
intent only and does not publish a package. The launch profile supports only
`local-server-headless`; runtime client preview, playable gameplay, editor
workflow, package publishing, runtime-backed C# blocks, and client package
stages are deferred until separate evidence exists.

Expected focused checks:

```sh
Tools/SagaPreviewGate/sagapreviewgate quickstart-check --out Build/TechnicalPreview/quickstart_report.json
Tools/SagaPreviewGate/sagapreviewgate accept --out-root Build/TechnicalPreview/Acceptance --bin-dir build/RelWithDebInfo-0.0.9/bin
Tools/SagaProjectKit/sagaproject validate --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out /tmp/project_validation_report.json
Tools/SagaProjectKit/sagaproject resolve --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out /tmp/project_resolution.json
Tools/SagaProjectKit/sagaproject doctor --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out /tmp/project_doctor_report.json
Tools/SagaLaunchLab/sagalaunch accept --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --launch-profile local-server-headless --out /tmp/acceptance_report.json --timeout-sec 5 --bin-dir build/RelWithDebInfo-0.0.9/bin
Tools/SagaProbe/sagaprobe latest --reports-dir samples/MultiplayerSandbox/Build/Reports --out /tmp/diagnostics_summary.json
Tools/SagaPackager/sagapack validate --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile technical-preview-server-headless --out /tmp/package_report.json
Tools/SagaPackager/sagapack stage --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile technical-preview-server-headless --out /tmp/package_report.json
Tools/SagaPackager/sagapack publish-check --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile technical-preview-server-headless --package-report /tmp/package_report.json --diagnostics-summary /tmp/diagnostics_summary.json --out /tmp/publish_report.json
Tools/SagaPackager/sagapack smoke --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile technical-preview-server-headless --package-report /tmp/package_report.json --out /tmp/package_smoke_report.json --timeout-sec 5 --bin-dir build/RelWithDebInfo-0.0.9/bin
Tools/SagaScript/sagascript analyze --source samples/MultiplayerSandbox/Scripts --out /tmp/sagascript
Tools/SagaScript/sagascript emit-bindings --source samples/MultiplayerSandbox/Scripts --out /tmp/sagascript
Tools/SagaScript/sagascript project-blocks --source samples/MultiplayerSandbox/Scripts --out /tmp/sagascript
```

The script fixtures provide fixture evidence for the source-preserving C#
analysis/projection path only:

- `DoorLogic.High.cs` maps to `Gameplay + High`.
- `DoorState.Low.cs` maps to `Gameplay + Low`.
- `AdvancedUnsupported.cs` maps unsupported C# to opaque read-only nodes.

Block J uses SagaPreviewGate to collect these focused checks into local
Technical Preview reports. It does not add product beta status, release
candidate status, production network readiness, full visual scripting,
arbitrary C# roundtrip, source-changing patch behavior, playable gameplay, full
collaboration, or enterprise readiness.
