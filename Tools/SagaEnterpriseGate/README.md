# SagaEnterpriseGate

SagaEnterpriseGate is the Target 3 / Enterprise-Evolvable Foundation local enterprise-evolvable evidence tool. It
reads local report files and emits deterministic report-only regression,
evidence gate, acceptance scenario, and closure evidence.

Commands:

```sh
Tools/SagaEnterpriseGate/sagaenterprisegate policy-regression --evidence-root Build/Enterprise --out Build/Enterprise/policy_regression_report.json
Tools/SagaEnterpriseGate/sagaenterprisegate evidence-gate --evidence-root Build/Enterprise --out Build/Enterprise/enterprise_evidence_report.json
Tools/SagaEnterpriseGate/sagaenterprisegate acceptance-scenario --evidence-root Build/Enterprise --out Build/Enterprise/enterprise_acceptance_report.json
Tools/SagaEnterpriseGate/sagaenterprisegate closure-check --evidence-root Build/Enterprise --out Build/Enterprise/enterprise_closure_report.json
```

Reports include `localOnly: true`, `networkExposure: "None"`,
`mutatesSource: false`, and `enforcement: "ReportOnly"`.

This tool does not enforce permissions, authenticate users, contact a network,
publish artifacts, or mutate project/source files.
