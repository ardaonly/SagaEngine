# Phase 14 Evidence

## Status

Implemented-Unverified

## Phase Scope

Read-Only Blocks Projection

Phase 14 adds the first metadata-only read-only Visual Blocks projection
artifact through the existing SagaScript CLI:

```bash
sagascript project-blocks --source <file-or-dir> --out <dir> --json
```

The command now writes `visual_blocks_projection_v1.json` alongside existing
projection artifacts.

Accepted boundary:

- generated from Saga-compatible C# compatibility-profile evidence;
- all emitted blocks and opaque regions have `editable: false`;
- source spans and source hashes are preserved as metadata;
- projectable, partially projectable, advanced opaque, and unsupported fixtures
  are covered by tests;
- unsupported C# emits diagnostics and unsupported diagnostic blocks;
- C# source files are not mutated;
- no editor UI, block editing, source patching implementation, runtime/server
  behavior change, package/distribution output, SDE work, or full visual
  scripting claim is included.

## Changed Files

- `Tools/SagaScript/src/Models.cs`
- `Tools/SagaScript/src/SagaWeaverArtifacts.cs`
- `Tools/SagaScript/src/Program.cs`
- `Tools/SagaScript/tests/test_sagascript_cli.py`
- `docs/architecture/CSHARP_VISUAL_BLOCKS_COMPATIBILITY_PROFILE.md`
- `docs/internal/PHASE_STATUS_MATRIX.md`
- `docs/internal/phase-evidence/PHASE_14/EVIDENCE.md`
- `docs/internal/phase-evidence/PHASE_14/commands.log`
- `docs/internal/phase-evidence/PHASE_14/changed_files.txt`
- `docs/internal/phase-evidence/PHASE_14/known_limitations.md`
- `docs/internal/phase-evidence/PHASE_14/verification_result.json`

## Verification Commands

- `nix-shell --run "python3 Tools/SagaScript/tests/test_sagascript_cli.py"`
- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 14`

## Command Results

The SagaScript CLI tests pass and include Phase 14 coverage for projectable,
partially projectable, advanced opaque, and unsupported C# fixture categories.
Final gate results are recorded in `commands.log`.

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
- [x] SagaScript projection artifact behavior was checked.
- [x] Unsupported behavior is not hidden.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

SagaScript now emits `visual_blocks_projection_v1.json` from `project-blocks`,
and fixture tests prove projectable, partially projectable, advanced opaque,
and unsupported C# produce read-only metadata without source mutation.
Maintainer verification, Visual Blocks editor UI, source patching
implementation, block editing, runtime/server behavior changes, package output,
distribution output, and SDE work remain absent.
