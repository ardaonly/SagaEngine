# Phase 37 Known Limitations

- This phase is not verified.
- StarterArena distribution workflow evidence is limited to unpacked `sagaproject validate`, `sagascript analyze`, and `Saga --workflow-smoke`.
- The Product Shell workflow report is report-only; referenced developer-tree commands inside that report are not executed by the distribution smoke.
- Packaged `SagaRuntime --starter-arena-smoke` is blocked because the current packaged binary enters normal client startup, attempts UDP transport setup, and does not produce the requested runtime smoke report.
- Packaged `SagaEditor --inspect-project` is blocked because the current packaged binary reports `unknown argument '--inspect-project'`.
- Visual Blocks workflow execution is not part of Phase 37.
- Full gameplay readiness is not claimed.
- Full editor workflow is not claimed.
- Full Visual Blocks UI is not claimed.
- Production readiness is not claimed.
- Enterprise readiness is not claimed.
- Verified final release status is not claimed.
- Full distribution verification is not claimed.
- No phase is marked `Verified`.
