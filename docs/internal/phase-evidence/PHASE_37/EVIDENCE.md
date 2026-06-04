# Phase 37 Evidence

## Status

Implemented-Unverified

## Phase Scope

Distribution StarterArena Workflow Smoke

Phase 37 extends the Linux distribution smoke to run a limited StarterArena
workflow from the unpacked archive tree only:

```txt
/tmp/saga_dist_smoke/Saga/
```

The smoke does not use repository tools, repository paths, repository build
directories, or repository samples.

## Changed Files

See `changed_files.txt`.

## Verification Commands

See `commands.log`.

## Command Results

- `scripts/smoke-linux-saga-dist` exits `0` with status `passed-with-limitations`.
- Archive checksum, archive root, unpack, file presence, executable-bit, and tool help checks pass.
- Unpacked `sagaproject validate` passes for `Saga/samples/StarterArena/StarterArena.sagaproj`.
- Unpacked `sagascript analyze` passes for `Saga/samples/StarterArena/Scripts`.
- Unpacked `Saga --workflow-smoke` writes a Product Shell workflow report.
- Packaged `SagaRuntime --starter-arena-smoke` is recorded as blocked.
- Packaged `SagaEditor --inspect-project` is recorded as blocked.

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
- [x] StarterArena workflow commands were checked from the unpacked archive tree.
- [x] Runtime and Editor blockers are recorded honestly.
- [x] Unsupported behavior is not hidden.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

Phase 37 has real unpacked-distribution StarterArena workflow smoke evidence for
project validation, SagaScript analysis, and Product Shell workflow report
generation. Runtime and Editor packaged workflow blockers remain recorded, and
maintainer verification has not occurred.
