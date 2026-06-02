# Policy Regression Suite Phase 117

Phase 117 adds `Tools/SagaEnterpriseGate` with a local regression command:

```sh
Tools/SagaEnterpriseGate/sagaenterprisegate policy-regression --evidence-root <dir> --out <policy_regression_report.json>
```

The command reads existing JSON reports from the evidence root and checks for:
policy allow, policy deny, dangerous-operation review, restricted slice
evidence, hidden source classification, publish blocked by policy, stale review
approval, hidden restricted export block, and WorkspaceHub report-only
invariants.

The output is deterministic local evidence with `schemaVersion`, `tool`,
`command`, `status`, `checks`, `summary`, `diagnostics`, `localOnly: true`,
`networkExposure: "None"`, `mutatesSource: false`, and
`enforcement: "ReportOnly"`.

This is regression evidence only. It does not enforce permissions,
authenticate users, contact a network, publish artifacts, or mutate source.
