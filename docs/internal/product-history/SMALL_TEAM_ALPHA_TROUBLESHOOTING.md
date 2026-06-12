# Small-Team Alpha Troubleshooting

Status: Milestone 94 onboarding evidence for local report failures.

## MissingEvidence

`MissingEvidence` means a required report file was not found under the evidence
root. Generate or copy the focused evidence report into `Build/SmallTeamAlpha`
and rerun the gate.

## Blocked Or Failed Evidence

`Blocked` and `Failed` statuses must be fixed at the producing tool or accepted
as residual debt in the closure report. The AlphaGate closure must not silently
treat them as passed.

## Deferred Evidence

Deferred gameplay expansion, ClientHost preview, and editor UI are expected
non-passing rows. Their presence is not a failure by itself, but the closure
must keep them visible as debt.

## Non-Claims

- no product beta
- no release candidate
- no production MMO server
- no complete visual scripting
- no arbitrary C# roundtrip
- no enterprise readiness
