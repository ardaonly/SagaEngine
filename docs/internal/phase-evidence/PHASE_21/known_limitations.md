# Phase 21 Known Limitations

- This phase is not verified.
- No public product claim should be derived from this file.
- Phase 21 is a docs/evidence-only workflow contract.
- No Product Shell dashboard workflow is implemented by this phase.
- No UI wiring is added for project validation, runtime smoke, SagaScript,
  Visual Blocks CLI evidence, server smoke, diagnostics, or package preflight.
- Existing `Apps/Saga` code is a product shell boundary, not a completed
  launcher/dashboard workflow.
- SagaEditor owns future editing and inspection UI.
- Visual Blocks evidence remains CLI-only and is not Visual Blocks editor UI.
- `scripts/package-linux-saga` remains preflight-only and is not package or
  distribution readiness.
- Failed tools, missing reports, and package preflight failures must be shown
  honestly by any future Product Shell UI.
- Phase 21 does not modify Runtime, Server, SagaScript, SDE, Visual Blocks
  editor UI, Editor implementation, package/distribution scripts,
  StarterArena gameplay, SagaRuntime smoke paths, CSharpScriptHost, or
  collaboration server behavior.
