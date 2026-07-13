# Current Architecture Status

> Last updated: 2026-07-13

SagaEngine is a structured engine/toolchain codebase with bounded local
evidence and explicit non-claims. It is not product beta, not release candidate,
and not production-ready.

## Current State

- Engine, runtime, server, editor, tool, and test code are present.
- Runtime, server authority, assets, editor boundaries, and packaging each have
  focused local coverage.
- The full raw CTest suite is not a current pass signal.
- Heavy stress/load checks are opt-in and unresolved.
- Full editor, playable runtime, production networking, production renderer,
  public SDK, and release packaging remain open.
- Source-truth and runtime-read evidence remains report-only. The retired
  standalone client host is not a supported path, and this evidence does not
  prove Client Preview, Runtime gameplay, Server gameplay, asset import, or
  asset cook behavior.

## Claim Boundary

Use current product docs for product claims. Use architecture summaries for
layer, ownership, source-truth, and dependency claims. Evidence reports and
inventory docs describe what was checked; they do not turn open product gaps
into implemented capability.

## References

- [Testing summary](TESTING_AND_EVIDENCE.md)
- [Architecture overview](ARCHITECTURE_OVERVIEW.md)
- [Claim and evidence policy](CLAIM_AND_EVIDENCE_POLICY.md)
