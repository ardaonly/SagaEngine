# Enterprise Security Limitations

## Status

Phase 123 freezes the security and governance limitations for Hedef 3. The
foundation is local/report-only evidence. It is not an enterprise security
implementation.

## Data Exposure Matrix

| Area | Current evidence | Exposure limit | Stronger claim blocked |
|---|---|---|---|
| Project slices | Slice and restricted resolution reports | Local files and reports can still be inspected by a local user. | no secure source protection |
| Source visibility | Visibility classifications and filtered reports | Classification is not encryption, DRM, or OS-level access control. | no secure redaction |
| WorkspaceHub | Local report-only commands | No authenticated transport, identity provider, or organization account model. | no cloud workspace |
| Presence | Presence redaction report | Redaction is evidence transformation, not a secure realtime channel. | no secure cloud platform |
| Audit log | Local audit report and hash diagnostics | Local artifacts are not tamper-proof production audit storage. | no compliance-ready audit |
| Review approval | Metadata-only approval report | Approval does not mutate source and does not enforce real permissions. | no permission system implemented |
| Publish policy | Publish check consumes policy/review evidence | Publish artifacts are not protected by signing, encryption, or secure export storage. | no artifact export security |
| Restricted export | Local restricted export report | Hidden or restricted content is omitted from the report, not cryptographically protected. | no DRM/encryption |
| Governance panel | Local dashboard model report | The panel is not auth UI, RBAC UI, or security management UI. | no full security model |

## Local-Only Limitations

- Evidence commands read local JSON, Markdown, and fixture data.
- Reports declare `localOnly: true`, `networkExposure: "None"`,
  `mutatesSource: false`, and `enforcement: "ReportOnly"` when the tool schema
  supports those fields.
- Local users with filesystem access can inspect source, reports, fixtures, and
  generated artifacts outside the modeled policy view.
- No local command proves account security, real permissions, RBAC, compliance,
  secure source protection, secure redaction, DRM, encryption, or artifact
  export security.

## Workspace-Server Limitations

- WorkspaceHub evidence is a local/team-server-oriented prototype model.
- No cloud transport, authenticated identity, account login, tenant isolation,
  organization administration, realtime authorization, or production audit
  storage is proven.
- No Runtime, Server, ClientHost, Editor UI, or Qt UI proof is implied by
  WorkspaceHub reports.
- Presence and Team Room outputs are filtered report artifacts, not proof that
  unauthorized clients never receive data over a real transport.

## Non-Encrypted Artifact Limitations

- Project reports, package reports, publish reports, and restricted export
  reports are plain local JSON.
- Hidden source is excluded or summarized in specific report outputs, but source
  files remain present in the local workspace unless a different system removes
  or protects them.
- Publish and restricted export checks are gates over evidence. They do not
  sign, encrypt, watermark, revoke, or enforce downstream access to artifacts.

## Explicit Non-Claims

- no enterprise readiness
- no production readiness
- no cloud security
- no auth/account security
- no real permissions
- no RBAC
- no production audit tamper-proofing
- no compliance
- no secure source protection
- no secure redaction
- no DRM/encryption
- no artifact export security
- no Runtime proof
- no Server proof
- no ClientHost proof
- no Editor UI proof
- no Qt UI proof
- no raw full CTest
- no heavy stress
- no real transport proof

## Future Hardening List

Future work must provide separate evidence before stronger claims:

- authenticated identity and account boundary
- server-side authorization before data serialization
- tenant/workspace isolation model
- cryptographic artifact protection where required
- tamper-evident audit storage outside local mutable files
- secure transport tests with restricted clients
- publish artifact signing and provenance
- policy regression coverage in CI
- raw full CTest evidence when stable
- heavy stress and real transport stress evidence when scoped

## Exit Criteria

Phase 123 is closed when these limitations are referenced by DocGuard and the
Phase 125 closure checkpoint, and when positive enterprise/security/compliance
claims remain blocked unless separate evidence proves them.
