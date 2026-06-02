# Saga Enterprise Threat Model v0

## Status

Phase 97 defines enterprise-evolvable threat categories. It does not implement
security controls, cryptography, penetration testing, zero-trust, compliance,
auth, login, enterprise RBAC, permission enforcement, or cloud infrastructure.

## Threat Matrix

| Threat id | Category | Severity | Current mitigation status | Deferred mitigation status | Later phase reference | Forbidden claim impact | Evidence required before stronger claim |
|---|---|---|---|---|---|---|---|
| `ENT-THREAT-001` | unauthorized source visibility | High | Source visibility is documented as a future policy boundary; current local tools can expose source metadata. | Project Slice and restricted resolution remain deferred. | Phase 104-108 | No complete source redaction security, no full security model. | Slice schema, restricted project view evidence, policy-scoped source visibility reports. |
| `ENT-THREAT-002` | presence information leakage | Medium | Collaboration model is local/offline and does not claim restricted presence. | Presence redaction and WorkspaceHub policy adapter remain deferred. | Phase 109-112, Phase 118-121 | No cloud workspace, no full collaboration. | Redacted presence report and policy adapter evidence. |
| `ENT-THREAT-003` | unauthorized artifact edits | High | Local collaboration reports classify locks/reviews as metadata evidence only. | Real permission enforcement remains deferred. | Phase 113-117 | No permission system implemented, no enterprise RBAC. | Policy-backed transaction gate and audit evidence. |
| `ENT-THREAT-004` | stale approval bypass | Medium | Patch review metadata is non-mutating and local/report-only. | Approval freshness and publish integration remain deferred. | Phase 113-117 | No production publish approval claim. | Stale approval diagnostics and publish gate policy evidence. |
| `ENT-THREAT-005` | publish gate bypass | High | Publish checks consume local evidence, not enterprise policy. | Policy-aware publish gate remains deferred. | Phase 113-117 | No production-ready packaging or production network readiness. | Publish policy denial/approval reports. |
| `ENT-THREAT-006` | private asset exposure | High | Asset workflow currently records missing source-of-truth debt. | Private asset policy and restricted asset views remain deferred. | Phase 104-108 | No secure asset redaction claim. | Asset source truth, policy-scoped asset report, restricted export evidence. |
| `ENT-THREAT-007` | restricted script/source exposure | High | SagaScript projection is local and report-only. | Restricted source filtering remains deferred. | Phase 104-108 | No secure cloud platform, no full security model. | Policy-scoped script/source report and restricted client view evidence. |
| `ENT-THREAT-008` | audit log tampering | High | No audit log implementation is claimed. | Audit log model and append-only evidence remain deferred. | Phase 113-117 | No compliance-ready or legal audit claim. | Audit event schema, tamper diagnostic report, deterministic audit evidence. |
| `ENT-THREAT-009` | policy drift | Medium | Phase 99-103 will introduce local policy vocabulary and report-only evaluation. | Cross-tool policy consistency remains deferred. | Phase 103, Phase 122-125 | No full enterprise policy engine. | Stable policy diagnostics and evidence gate reports. |
| `ENT-THREAT-010` | workspace session spoofing | Medium | Workspace sessions are local model evidence and not security identity. | WorkspaceHub session policy and authenticated identity remain deferred. | Phase 109-112 | No auth system implemented, no zero-trust. | WorkspaceHub prototype evidence and identity boundary report. |

## Non-Goals

- cryptographic protocol design
- penetration testing
- auth/security implementation
- zero-trust claim
- SOC2/ISO/compliance-ready claim
- secure cloud platform claim
- production-ready enterprise platform claim

## Exit Criteria

This phase is complete when these threat IDs are available for later evidence
references and every stronger claim has an explicit evidence prerequisite.
