# Publish

> Last updated: 2026-05-26

Publish-related tooling can check selected project/package inputs and report
deterministic blockers. It is not a full release packaging or publishing
system.

## Current State

- Publish report schema version 1 is stable for current fields.
- Current hard blockers include deterministic project, package, and explicit
  diagnostics failures.
- Missing cooked assets, unsupported asset kinds, duplicate identity mappings,
  unresolved raw full CTest, and heavy checks remain warnings or deferred work.
- Release packaging and CI hard enforcement are absent.

## Background

- [Phase 10 closure](../recovery/phase-10-publish-gate/PHASE_10_CLOSURE_CHECKPOINT.md)
- [Publish profile policy](../recovery/phase-10-publish-gate/PHASE_10D_PUBLISH_PROFILE_POLICY.md)
- [Compatibility proof](../recovery/phase-10-publish-gate/PHASE_10E_PRODUCT_PACKAGING_COMPATIBILITY_PROOF.md)
