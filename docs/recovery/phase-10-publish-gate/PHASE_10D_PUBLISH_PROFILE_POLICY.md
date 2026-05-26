# Phase 10D Publish Profile Policy

> Last updated: 2026-05-26
> Status: Phase 10D profile policy defined
> Phase 10: Publish Gate / Product Packaging Reality Check

Phase 10D defines publish profile policy and hard-gate eligibility. This slice
does not add a broad profile implementation and does not enable new global hard
gates.

## Profile Policy

| Profile | Intended use | Required checks | Warning-only checks | Blockers | Deferred checks |
|---|---|---|---|---|---|
| `dev-local` | Local product iteration. | Project/package report generation. | Raw full CTest unresolved, heavy gates, cook/import gaps. | Existing deterministic project/package/explicit diagnostics blockers only. | CI enforcement, full product proof. |
| `package-smoke` | Focused package/runtime smoke. | Package manifests, generated manifests, runtime package smoke tests. | Missing heavy gates and raw full CTest. | Existing deterministic project/package blockers. | Full AssetPipeline cook/import and ClientHost consumption. |
| `shipping-report-only` | Product-facing publish report without release claim. | Schema v1 report and package/asset/identity coverage. | Missing cooked assets and unsupported asset kinds remain report-only. | Existing deterministic blockers only. | Hard release gate. |
| `shipping-full` | Future shipping policy target. | Same current report checks plus future explicit evidence inputs. | Raw full CTest unresolved and heavy gates unless explicitly provided. | Existing deterministic blockers only in Phase 10. | Release readiness, performance, stress/load, full product proof. |
| `CI-candidate` | Deterministic CI gate candidate. | Focused publish/package/runtime tests and Phase 9 normal local gate. | Raw full CTest unresolved and heavy gates. | Existing deterministic blockers and focused test failures. | Heavy/default raw full CTest CI. |
| `future-enterprise` | Later stricter release governance. | Not defined in Phase 10. | All missing enterprise evidence. | None added in Phase 10. | Security, compliance, signing, provenance, broad platform matrix. |

## Hard-Gate Eligibility

A check can block only when it is:

- deterministic
- locally test-covered
- actionable
- independent of unresolved raw full CTest or heavy gate debt unless the
  profile explicitly requires those gates
- not treating a missing feature as a product failure
- first represented as warning/report-only where practical

## Safe Blockers

Safe blockers in Phase 10 are the existing blockers:

- missing or invalid project manifest
- missing or invalid package manifest
- package kind mismatch
- missing explicit external diagnostics report
- malformed explicit external diagnostics report
- explicit external diagnostics with blocking diagnostics

These blockers are deterministic and do not depend on raw full CTest,
`StressTests`, `ReplicationTests`, or incomplete AssetPipeline/runtime/client
features.

## Warning-Only Checks

Warning-only or report-only:

- raw full CTest unresolved
- stress/load not run
- heavy `ReplicationTests` not run
- source cook/import not implemented
- ClientHost package asset catalog consumption missing
- RuntimeServiceRegistry asset service missing
- full MultiplayerSandbox/product proof missing
- missing cooked asset coverage
- unsupported asset kind coverage
- duplicate identity mapping coverage

## Hard-Gate Decision

No new hard gate is enabled in Phase 10D. Existing deterministic blockers stay
in place. All other product packaging gaps remain warning-only or deferred.

## Non-Goals

Phase 10D does not add a broad profile system, CI hard enforcement, release
gate, source cook/import, RuntimeServiceRegistry asset service, ClientHost
asset consumption, raw full CTest requirement, or heavy-gate default.

## Phase 10E Decision

Phase 10E compatibility proof is safe because the profile policy does not
overclaim and the focused package/publish/runtime tests are bounded.

