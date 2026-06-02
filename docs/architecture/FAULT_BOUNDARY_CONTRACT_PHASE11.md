# FaultBoundary Contract Phase 11

Phase 11 adds minimal runtime fault classification and diagnostics reporting
where real subsystem boundaries already exist. It does not execute recovery.

## Repository Finding

Existing boundaries suitable for a minimal contract are:

- `RuntimeServiceRegistry` and `RuntimeServiceLifecycle`
- `RuntimeStartupPreflight`
- runtime startup/session diagnostics
- `ZoneServer` lifecycle diagnostics
- `DiagnosticSystem` operational report construction

These systems already return deterministic result structs and diagnostics.
Phase 11 follows that pattern instead of adding exception isolation or fake
runtime recovery.

## Contracts

`FaultPolicy` values:

- `Recoverable`
- `NonRecoverable`
- `DeferToCaller`
- `ReportOnly`
- `BlockStartup`

`RecoveryAction` values:

- `None`
- `RetryAllowed`
- `DisableSubsystem`
- `ShutdownRequested`
- `BlockStartup`
- `ReportOnly`

`FaultReport` records a bounded diagnostic event:

- sequence
- fault id
- subsystem
- severity
- policy
- recommended action
- message
- diagnostic code
- sorted metadata

No timestamp is recorded. Report construction remains deterministic and
read-only.

## Diagnostics Shape

Operational reports include an additive `faults` section:

```json
{
  "faults": {
    "reports": [],
    "summary": {
      "faultCount": 0,
      "droppedFaultCount": 0
    }
  }
}
```

`DiagnosticReport.summary.faultReportCount` mirrors the retained fault count for
quick scanning.

## Evidence

Focused test target:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'FaultBoundaryTests|DiagnosticReportTests' --output-on-failure --timeout 30 -j 1
```

`FaultBoundaryTests` covers policy classification, sorted metadata, diagnostics
storage, report serialization, empty report shape, bounded retention, and the
fact that recommended recovery is reported only.

## Non-Goals

This phase does not implement a full fault-isolation framework, automatic crash
recovery, subsystem fake recovery, production fault tolerance, chat/inventory
fault handling, combat recovery, or a broad runtime/server rewrite.
