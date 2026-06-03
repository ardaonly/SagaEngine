# Phase 20 Evidence

## Status

Implemented-Unverified

## Phase Scope

Saga Schema Package Boundary

Phase 20 defines the boundary between standalone SDE generic artifacts and
future Saga-specific schema/package consumption. It is a docs/evidence-only
batch.

## Boundary Document

This phase adds:

- `docs/architecture/SAGA_SCHEMA_PACKAGE_BOUNDARY.md`

The document records current reality:

- there is no dedicated Saga schema package tree yet;
- `share/saga/schemas/` was not created;
- fake schema package folders were not created;
- `Tools/SystemDefinitionEngine/examples/saga/` was not created.

It also records the dependency direction:

```txt
Saga consumers may depend on SDE outputs and public SDE::Core APIs.
SDE Core must not depend on Saga consumers.
```

## Current Surfaces Recorded

The current Saga-facing SDE surfaces are adapter or consumer surfaces outside
SDE Core:

- CMake can wire Saga targets to `SDE::Core` when `SAGA_WITH_SDE=ON`;
- `SagaEditorCompositionLib` is documented as Saga-specific artifact generation
  outside the standalone SDE package tree;
- `SagaEditorLib` and `SagaProductLib` may link `SDE::Core` when SDE is enabled;
- `scripts/package-linux-saga` checks for an `sde` CLI but remains preflight
  only;
- samples contain Saga project/package metadata, but not a standalone Saga
  schema package.

## Non-Claims

This phase does not claim:

- package readiness;
- distribution readiness;
- runtime/editor schema consumption;
- final Saga schema package layout;
- Visual Blocks, SagaScript, CSharpScriptHost, StarterArena gameplay, runtime,
  server, editor, or package/distribution implementation.

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
- [x] Current absence of a dedicated Saga schema package tree is documented.
- [x] Dependency direction is documented.
- [x] No phase is marked `Verified`.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

The Saga schema package boundary is documented honestly without creating a fake
schema package tree. The document records current surfaces, dependency
direction, non-claims, and future package shape as future work only. Maintainer
verification, package readiness, distribution readiness, runtime/editor
consumption, SagaScript, Visual Blocks, CSharpScriptHost, StarterArena gameplay,
and SDE Core coupling remain absent.
