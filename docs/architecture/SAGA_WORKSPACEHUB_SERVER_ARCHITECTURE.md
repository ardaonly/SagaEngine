# SagaWorkspaceHub Architecture Checkpoint

Hedef 3 Phase 109 defines SagaWorkspaceHub as a local report coordinator for
workspace evidence. Phase 110 starts with a CLI, not a resident server process.

SagaWorkspaceHub may read:

- a local `.sagaproj` manifest;
- SagaProjectKit restricted project resolution reports;
- SagaPolicyKit policy evaluation reports;
- SagaViewKit view slice compatibility reports.

SagaWorkspaceHub may write deterministic JSON reports under an explicit output
path supplied by the caller. The first report set is:

- `workspacehub_summary_report.json`;
- `workspacehub_policy_adapter_report.json`;
- `workspacehub_slice_scoped_view_report.json`.

Every report is local-only, report-only, and source-preserving:

- `localOnly = true`
- `networkExposure = "None"`
- `mutatesSource = false`
- `enforcement = "ReportOnly"`

## Phase 109 Boundary

The architecture checkpoint separates three concepts:

- local workspace report coordination;
- future local loopback transport;
- remote team or hosted workspace services.

Only the first concept is implemented in Phase 110. The tool reads local files,
adapts report decisions, and emits new reports. It does not create accounts,
authenticate users, hold durable workspace state, or protect files from direct
filesystem access.

## Phase 110 CLI Shape

The Phase 110 command surface is:

```sh
Tools/SagaWorkspaceHub/sagaworkspacehub summarize --project <.sagaproj> --slice-resolution <report> --policy-report <report> --view-compatibility <report> --out <workspacehub_summary_report.json>
```

This command validates the existence and parseability of local inputs and emits
a deterministic workspace summary. It stops before server lifecycle, network
binding, durable workspace state, background processes, or Editor integration.

## Phase 111 Policy Adapter

The policy adapter consumes SagaPolicyKit reports and maps policy decisions into
WorkspaceHub report dispositions:

| SagaPolicyKit decision | WorkspaceHub adapter decision |
|---|---|
| `Allow` | `AllowedByReport` |
| `Deny` | `DeniedByReport` |
| `RequiresReview` | `RequiresReviewByReport` |
| missing report or `MissingEvidence` | `MissingPolicyEvidence` |
| malformed report or unknown decision | `UnknownPolicyState` |

Rejected before mutation means the WorkspaceHub report marks a simulated
operation as rejected. It does not enforce filesystem permissions, authenticate
users, or provide a security boundary.

## Phase 112 Slice-Scoped View

The slice-scoped project view consumes SagaProjectKit restricted resolution and
SagaViewKit view compatibility reports. It may list visible resources and
disclose hidden or restricted counts. It keeps `SummaryOnly`,
`OpaqueRestricted`, and `Hidden` resources as placeholders and does not include
source text for those resources.

## Deferred Server Mode

No `serve` command exists in the Phase 110 implementation. If a future phase
adds a server process, it must:

- bind only to loopback addresses;
- expose read-only report endpoints;
- reject public binds;
- avoid production HTTP API claims;
- preserve `mutatesSource = false` for report-only endpoints;
- remain outside cloud, account, auth, enterprise RBAC, and remote team server
  scope.

## Non-Goals

SagaWorkspaceHub Phase 109-112 does not implement cloud-hosted workspace behavior,
realtime collaboration, websocket transport, account login, auth, security
identity, enterprise RBAC, actual permission checks, CRDT or operational
transform merge, source protection, Runtime gameplay, Server gameplay,
ClientHost, Editor UI, Qt UI, SagaScript patch behavior, package format changes,
or later workflow gates.
