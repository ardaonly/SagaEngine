# SagaEngine Docs

> Read this first.
> Last updated: 2026-05-26
> Current truth: Foundation Established. SagaEngine is not Product Beta and not
> Release Candidate.

This directory keeps the detailed recovery evidence in place and adds a short
entry layer for deciding what to read next.

## Current Classification

The current accepted classification is **Foundation Established**. That means
SagaEngine has evidence for several engine foundations, but the evidence does
not support Product Beta, Release Candidate, production-ready, or shipping-ready
claims.

Start with
[PHASE_11_CLOSURE_CHECKPOINT.md](recovery/phase-11-scoring/PHASE_11_CLOSURE_CHECKPOINT.md)
for the current truth. Use the other docs as supporting evidence, not as newer
claims.

## Reading Paths

| Need | Start here | Detail source |
|---|---|---|
| Fast status | [Current status](architecture/CURRENT_STATUS.md) | [Phase 11 closure](recovery/phase-11-scoring/PHASE_11_CLOSURE_CHECKPOINT.md) |
| Recovery history | [Roadmap index](roadmaps/README.md) | [Engine recovery roadmap](roadmaps/ENGINE_RECOVERY_ROADMAP.md) |
| Test and gate status | [Testing index](testing/README.md) | [Test suites](testing/TEST_SUITES.md) |
| Post-recovery work | [Phase 11E roadmap](recovery/phase-11-scoring/PHASE_11E_POST_RECOVERY_ROADMAP.md) | Product Vertical Proof is the next macro direction. |
| Older technical specs | This index, then the file you need | [Client replication spec](ClientReplicationFormalSpec.md), [Asset streaming](AssetStreamingImplementation.md), [Dependency graph](DependencyGraph.md) |
| Iteration audit trail | [Dev index](dev/README.md) | [Iteration notes](dev/ITERATION_NOTES.md) |

## What Is Proven

| Proven foundation evidence | Not proven yet |
|---|---|
| Runtime startup/lifecycle foundation exists. | Product Beta readiness. |
| Server authoritative movement foundation exists. | Release Candidate readiness. |
| Asset/package/runtime readiness foundation exists. | raw full CTest pass evidence. |
| Publish readiness report-first gate exists with selected deterministic blockers. | heavy gates, stress/load/performance readiness, or heavy replication readiness. |
| Editor public Qt exposure is reduced and guarded with explicit remaining debt. | full AssetPipeline source import/cook. |
| Phase 9 normal local gate passed 36/36 with heavy labels excluded. | full ClientHost/runtime asset consumption proof. |
| Phase 10 focused package/publish/runtime compatibility passed 12/12. | full playable product proof, full editor product readiness, CI hard enforcement, or release packaging proof. |

## Index Rules

- Recovery Phase files are historical evidence and stay in the recovery
  archive.
- Roadmaps describe intent unless a closure or test record proves the claim.
- raw full CTest and heavy gates remain unresolved unless a later evidence file
  explicitly records a complete passing run.
- Future implementation should move through the post-recovery roadmap, starting
  with Product Vertical Proof, while preserving these non-claims until new
  evidence changes them.
