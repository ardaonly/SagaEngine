# Current Architecture Status

> Last updated: 2026-07-13

SagaEngine is a structured engine/toolchain codebase with bounded local
evidence and explicit non-claims. It is not product beta, not release candidate,
and not production-ready.

## Current State

- Engine, runtime, reusable server foundations, editor, tool, and test code are
  present. The legacy snapshot-probe server app has been retired; no product
  dedicated-server executable is currently implemented or shipped.
- Runtime, `SagaServerLib` authority foundations, assets, editor boundaries,
  and packaging each have focused local coverage.
- The app spine is explicit: `Saga` owns product orchestration and typed
  process handoff, `SagaEditor` is a thin host over Editor-owned behavior, and
  `SagaRuntime` is a standalone host with no Product, Editor, Sandbox,
  EditorLab, or server-library dependency. Generic Runtime project execution
  remains unsupported.
- The default Linux candidate has an exact three-application/three-CLI
  whitelist with blocking extra-executable checks and file-level inventory.
  Native dependency closure and clean-machine evidence remain open.
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
