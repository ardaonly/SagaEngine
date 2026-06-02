# SagaPolicyKit

SagaPolicyKit is the local, report-only policy evaluator for Target 3 / Enterprise-Evolvable Foundation Phase
100-103.

It does not authenticate users, enforce permissions, protect files, contact a
network, run WorkspaceHub, execute dangerous operations, or claim enterprise
readiness.

```sh
Tools/SagaPolicyKit/sagapolicy evaluate --policy policy.json --request request.json --out policy_evaluation_report.json
```

Every report includes:

- `mutatesSource: false`
- `enforcement: "ReportOnly"`

Supported decisions:

- `Allow`
- `Deny`
- `Warn`
- `RequiresReview`
- `NotApplicable`
- `MissingEvidence`
- `UnknownSubject`
- `UnknownAction`
- `UnknownResource`
