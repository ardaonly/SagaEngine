# Saga Performance And Usability Budgets

> Status: v0 workflow budget report contract

This document defines initial budgets and report semantics for alpha workflow
discipline. It is not a benchmark suite, profiler, load test, or production
performance readiness claim.

## Budget Table

| Budget id | Workflow | v0 budget |
|---|---|---|
| `editor-open` | Editor open budget | 5000 ms |
| `project-validation` | Project validation budget | 1000 ms |
| `script-analysis` | Script analysis budget | 2500 ms |
| `preview-launch` | Preview launch budget | 8000 ms |
| `package-check` | Package check budget | 4000 ms |
| `diagnostics-summary` | Diagnostics summary budget | 1000 ms |
| `collaboration-update` | Collaboration update budget | 1000 ms |

## Report Contract

`performance_budget_report.json` records:

- `schemaVersion`;
- tool and command;
- overall status;
- budget id;
- workflow label;
- budget in milliseconds;
- optional measured milliseconds;
- status for each budget;
- deterministic diagnostics.

Missing measurements are reported as `MeasurementMissing`. They are not silent
passes. A value over budget is reported as `BudgetExceeded`.

## Non-Claims

- no product beta;
- no release candidate;
- no production MMO server;
- no complete visual scripting;
- no arbitrary C# roundtrip;
- no enterprise readiness.

Additional boundaries:

- no production performance readiness;
- no benchmark lab;
- no heavy stress;
- no long soak;
- no bot swarm;
- no real transport stress;
- no raw full CTest claim.
