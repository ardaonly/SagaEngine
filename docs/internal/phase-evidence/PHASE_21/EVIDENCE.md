# Phase 21 Evidence

## Status

Implemented-Unverified

## Phase Scope

Product Shell v1

Phase 21 defines the Product Shell workflow as a docs/evidence-only contract
over existing entry points. It records the Product Shell as a
launcher/dashboard/workflow router and explicitly avoids claiming a completed
dashboard implementation.

## Contract Document

This phase adds:

- `docs/architecture/SAGA_PRODUCT_SHELL_WORKFLOW_CONTRACT.md`

The document records current reality:

- `Apps/Saga` already contains project create/open, target preparation, process
  launcher, session model, script gate, package staging, publish readiness, and
  editor facade boundaries;
- `Apps/Editor` remains the editor launcher;
- `Apps/EditorLab` remains a scenario/development shell;
- the first workflow contract delegates to existing CLI and process entry
  points for project validation, runtime smoke, SagaScript analyze/compile,
  CLI-only Visual Blocks proof, server smoke, diagnostics, and package
  preflight;
- failed tools, missing reports, and package preflight failure must remain
  visible.

## Existing Entry Points Recorded

- `nix-shell --run "Tools/SagaProjectKit/sagaproject validate --project samples/StarterArena/StarterArena.sagaproj --out /tmp/starter_arena_validate.json"`
- `build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke ...`
- `Tools/SagaScript/sagascript analyze`
- `Tools/SagaScript/sagascript compile`
- `compatibility-profile -> project-blocks -> plan-block-edit -> apply-block-edit -> analyze -> compile`
- `build/RelWithDebInfo-0.0.9/bin/MultiplayerSandboxHeadless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-server-smoke ...`
- `/tmp/*_smoke.json`, SagaScript diagnostics, project validation reports, and
  server diagnostics directories;
- `scripts/package-linux-saga`, documented as preflight-only and not package or
  distribution readiness.

## Non-Claims

This phase does not claim:

- completed Product Shell dashboard workflow;
- full editor;
- Visual Blocks editor UI;
- package builder;
- distribution pipeline;
- enterprise workspace server;
- runtime engine;
- C# compiler;
- SDE compiler;
- runtime/server/SagaScript/SDE/package script behavior changes.

## Changed Files

See `changed_files.txt`.

## Verification Commands

See `commands.log`.

## Command Results

The recorded local checks passed in this batch. The phase remains
`Implemented-Unverified` because maintainer verification has not occurred.

## Required Files

- `EVIDENCE.md`: present
- `commands.log`: present
- `changed_files.txt`: present
- `known_limitations.md`: present
- `verification_result.json`: present

## Manual Checks

- [x] Public docs do not overclaim.
- [x] Known limitations are documented.
- [x] No placeholder is presented as shipped behavior.
- [x] Existing `Apps/Saga` shell/product host boundary is acknowledged without
  claiming full workflow implementation.
- [x] Shell/editor/tool boundaries are documented.
- [x] Missing dashboard/editor/package/distribution functionality is recorded.
- [x] CLI-only Visual Blocks proof is not described as editor UI.
- [x] Package preflight failure is documented as a limitation, not a pass.
- [x] No phase is marked `Verified`.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

The Product Shell workflow contract exists and documents existing real entry
points honestly. It records the current `Apps/Saga` boundary, delegates proof to
CLI/tool reports, requires command/exit/report/diagnostics visibility, and
records missing dashboard, editor UI, package builder, distribution pipeline,
enterprise server, runtime engine, compiler, and SDE compiler functionality as
limitations. Maintainer verification has not occurred.
