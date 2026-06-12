# Local Audit Chain Evidence v1

> Former title: Immutable Audit Log v1
> Status: Local report-only audit evidence adapter

This document adds a local, deterministic audit evidence adapter in
`Tools/SagaWorkspaceHub`.

Command:

```sh
Tools/SagaWorkspaceHub/sagaworkspacehub audit-log --events <audit_events.jsonl> --out <audit_report.json>
```

The input is JSONL. Each record may provide `eventId`, `eventType`, `actorRef`,
`targetRef`, `sequence`, `timestamp`, `previousRecordHash`, `recordHash`,
`evidenceRefs`, and `diagnostics`.

The output report contains:

- `schemaVersion`, `tool`, `command`, and `status`
- ordered `events`
- `summary.eventCount`
- `summary.countsByEventType`
- `summary.hashChainStatus`
- `localOnly: true`
- `networkExposure: "None"`
- `mutatesSource: false`
- `enforcement: "ReportOnly"`
- `diagnostics`

If record hashes are present, the tool checks only local hash-chain continuity:
an event `previousRecordHash` must match the prior ordered event `recordHash`.
This is local evidence checking only. It is not production tamper-proofing,
cryptographic audit security, compliance evidence, source protection, or a
networked audit service.
