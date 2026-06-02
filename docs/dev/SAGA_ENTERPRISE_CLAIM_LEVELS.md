# Saga Enterprise Evidence Vocabulary and Claim Levels

## Status

Phase 98 defines Hedef 3 claim discipline. It does not certify SagaEngine as
secure, compliant, production-ready, or enterprise-ready.

## Claim Levels

| Level | Meaning |
|---|---|
| `NotClaimed` | No product claim is made. |
| `Planned` | Work is roadmap-scoped but not evidenced. |
| `EvidenceDefined` | Evidence requirements are documented. |
| `ReportOnlyEvidence` | A tool or document reports model state without enforcement. |
| `LocalFocusedEvidence` | A bounded local test or CLI proves one behavior. |
| `BoundedImplementation` | A scoped implementation exists and is tested inside declared limits. |
| `DeferredWithEvidence` | The gap is explicit and backed by blocker evidence. |
| `Blocked` | The claim is blocked until prerequisite evidence exists. |
| `NotEnterpriseReady` | The work may be enterprise-evolvable but must not be called enterprise-ready. |

## Forbidden Positive Claims

These phrases must not appear as positive product claims without future evidence
outside Phase 96-103:

- enterprise-ready
- production-ready
- release candidate
- product beta
- secure cloud platform
- SOC2/ISO/compliance-ready
- auth system implemented
- permission system implemented
- full security model
- cloud workspace
- full collaboration
- raw full CTest passed
- heavy stress passed
- real transport stress passed
- zero-trust

Reviewed non-claim wording is allowed when the context clearly says the claim is
not implemented, deferred, blocked, not claimed, forbidden, or a future
prerequisite.

## Evidence Requirements

| Claim family | Minimum acceptable level in Phase 96-103 |
|---|---|
| Hedef 3 opening | `EvidenceDefined` |
| enterprise-evolvable vocabulary | `EvidenceDefined` |
| policy domain model | `ReportOnlyEvidence` |
| SagaPolicyKit local evaluation | `LocalFocusedEvidence` |
| project role profiles | `ReportOnlyEvidence` |
| dangerous operation gate | `ReportOnlyEvidence` |
| policy diagnostics | `ReportOnlyEvidence` |
| enterprise readiness | `NotEnterpriseReady` |
| security enforcement | `Blocked` |
| cloud/realtime collaboration | `Blocked` |

## Phase 98 Exit Criteria

Docs can describe enterprise-evolvable foundations only when they preserve
non-claims for enterprise readiness, security enforcement, cloud collaboration,
auth, permission enforcement, raw full CTest, heavy stress, and real transport
stress.
