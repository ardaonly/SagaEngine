# Phase 11A Scoring Rubric

> Last updated: 2026-05-26
> Status: Phase 11A scoring rubric established
> Phase 11: Scoring Re-Audit

Phase 11 scores SagaEngine from evidence, not ambition. Phase 0 through Phase
10 closure docs are accepted evidence unless a later audit contradicts them.
Docs-only plans, focused tests, and roadmap intent do not prove product or
release readiness by themselves.

## Scoring Categories

1. Runtime ownership and lifecycle
2. Server authority / multiplayer foundation
3. Networking / replication foundation
4. Asset/package/runtime readiness
5. AssetPipeline source import/cook readiness
6. Publish readiness / product packaging
7. Editor public API / de-Qt boundary
8. Editor scaffolding quarantine
9. Visual scripting / scripting / C# readiness
10. Documentation/code alignment
11. Local evidence gates / test health
12. CI/release readiness
13. Product/playable vertical proof
14. Architecture/ownership discipline
15. Overall recovery maturity

## Scoring Rubric

| Score | Meaning |
|---:|---|
| 0/10 | Absent, misleading, or unowned. |
| 1-2/10 | Scattered scaffold, mostly aspirational, little or no test evidence. |
| 3/10 | Basic scaffold exists, ownership unclear, evidence weak. |
| 4/10 | Partial foundation exists, major gaps remain, focused proof limited. |
| 5/10 | Foundation exists, meaningful tests/docs exist, not product-ready. |
| 6/10 | Foundation is coherent, bounded production-shaped path exists, important gaps remain. |
| 7/10 | Production-shaped subsystem with focused tests, ownership mostly clear, integration incomplete. |
| 8/10 | Mature local workflow, broad focused evidence, remaining debt explicit and bounded. |
| 9/10 | Mature, integrated, broadly verified, strong CI/local gate evidence, only minor debt remains. |
| 10/10 | Release-grade, stress/product/CI/release evidence proven, no major unresolved blockers. |

## Evidence Quality

The evidence quality levels below define how much each proof type can support a
score.

| Evidence quality | Meaning |
|---|---|
| Direct proof | The exact behavior or boundary is implemented and directly tested. |
| Focused proof | Narrow tests prove the named slice but not complete product behavior. |
| Partial proof | Some implementation and evidence exist, with major integration gaps. |
| Coarse proof | Broad tests or grouped binaries provide useful but low-resolution evidence. |
| Docs-only proof | Audit, plan, or classification only; cannot prove runtime behavior. |
| Report-only proof | Visibility/reporting exists without hard enforcement. |
| Explicitly deferred debt | The repo states the claim is intentionally not closed. |
| No proof | No reliable implementation or verification evidence found. |
| Contradicted/unsafe claim | A claim would conflict with accepted non-claims or missing evidence. |

## Classification Levels

| Classification | Required evidence |
|---|---|
| Prototype / research scaffold | Scattered systems and weak verification. |
| Foundation established | Multiple core foundations implemented with focused evidence and honest non-claims. |
| Foundation complete candidate | Most foundation tracks are coherent, integrated, and locally verified, with product blockers still explicit. |
| Product beta candidate | A playable/product vertical is proven, with package/runtime/client/server/editor workflow evidence. |
| Release candidate | Broad CI, raw full CTest or replacement policy, heavy gates, packaging, stress/load/performance, and product proof are all established. |

## Score Caps

- Raw full CTest unresolved caps local evidence gates / test health and
  CI/release readiness.
- Heavy gates unresolved cap stress/load/performance readiness and any release
  candidate classification.
- No full product/playable vertical proof blocks product beta candidate and
  release candidate.
- Full AssetPipeline source import/cook incomplete caps AssetPipeline source
  import/cook readiness.
- ClientHost/runtime asset consumption incomplete caps runtime product
  integration.
- RuntimeServiceRegistry asset service incomplete caps runtime service/product
  maturity.
- Full editor product readiness incomplete caps editor maturity.
- Docs-only evidence cannot exceed a moderate score alone.
- Focused tests do not prove full product readiness.
- No category may score 9+ without broad integration evidence.
- No release candidate classification is allowed while raw full CTest, heavy
  gates, and product proof remain unresolved.

## Non-Claims

Phase 11A does not score categories yet, implement post-recovery work, claim
product beta, claim release readiness, run raw full CTest, run heavy gates, or
fix blockers.

## Recommended Phase 11B

Proceed to Phase 11B evidence collection. Every later score must cite accepted
docs, code/modules, focused tests, normal local gate evidence, or explicit
deferred debt.
