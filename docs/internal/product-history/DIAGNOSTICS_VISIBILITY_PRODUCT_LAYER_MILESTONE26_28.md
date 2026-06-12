# Diagnostics Visibility Product Layer Milestone 26-28

> Block D evidence for Target 1 / Technical Preview Technical Preview product work.

Milestone 26-28 adds `Tools/SagaProbe`, a read-only CLI for turning diagnostics
artifacts into product-facing summaries and deterministic comparisons.

## Implemented Path

The supported commands are:

```text
sagaprobe summarize --input <report> --out <summary>
sagaprobe compare --baseline <report> --candidate <report> --out <diff>
sagaprobe latest --reports-dir <dir> --out <summary>
```

`summarize` writes `diagnostics_summary.json` and prints a compact console
summary. `compare` writes `report_diff.json`. `latest` recursively scans a
reports directory, filters unsupported JSON files, and selects the final
supported report by lexical relative path so selection is deterministic.

## Supported Reports

SagaProbe v1 supports:

- Engine operational and lifetime diagnostics reports;
- Engine reliability-style reports;
- SagaChaosLab reports;
- MultiplayerSandboxHeadless reports;
- SagaLaunchLab server launch reports;
- SagaLaunchLab acceptance reports.

Unsupported JSON fails with `Probe.Report.UnsupportedKind`. Invalid and missing
inputs fail with stable diagnostic codes.

## Summary Shape

`diagnostics_summary.json` contains:

- `schemaVersion`, `tool`, `command`, and `status`;
- source report path and detected report kind;
- counts for diagnostics, metrics, lifecycle records/events, faults, and leaks;
- deterministic metrics;
- critical and warning diagnostics;
- section presence for health, lifecycle, faults, chaos, headless, launch, and
  acceptance data.

`status` is `Passed`, `Attention`, or `Failed`. Deferred acceptance stages are
shown as warnings so product workflow users can see what remains deferred.

## Compare Shape

`report_diff.json` contains:

- baseline and candidate source metadata;
- added and removed diagnostics;
- exact numeric metric changes;
- missing sections;
- summary counts.

The comparison is exact and deterministic. It is not a timing trend model or
performance scoring system.

## Milestone 28 Bridge

The Milestone 28 bridge is CLI-only through `sagaprobe latest`. The current product
shell uses Qt application code, so editor shell presentation is intentionally
deferred until a safe non-invasive shell seam exists.

## Non-Claims

This layer does not provide a viewer surface, Qt panel, telemetry upload,
historical metrics store, release gate, packaged product proof, production
networking proof, or gameplay correctness proof.
