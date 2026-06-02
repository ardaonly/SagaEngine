# SagaProbe

SagaProbe is the Phase 26-28 diagnostics visibility CLI.

It reads supported SagaEngine diagnostics JSON artifacts and produces a stable
summary or comparison report.

```sh
Tools/SagaProbe/sagaprobe summarize --input Build/Reports/headless_server_report.json --out Build/Reports/diagnostics_summary.json
Tools/SagaProbe/sagaprobe compare --baseline run_a.json --candidate run_b.json --out report_diff.json
Tools/SagaProbe/sagaprobe latest --reports-dir Build/Reports --out Build/Reports/diagnostics_summary.json
```

Supported inputs include Engine diagnostics reports, reliability reports,
SagaChaosLab reports, MultiplayerSandboxHeadless reports, and SagaLaunchLab
server or acceptance reports.

The tool is read-only. It does not create viewer surfaces, add Qt panels,
upload telemetry, run launch flows, or change project/runtime/editor files.
