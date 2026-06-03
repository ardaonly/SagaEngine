# Phase 12 Evidence

## Status

Implemented-Unverified

## Phase Scope

Server-Authoritative Sample v1

Phase 12 adds one bounded, socket-free StarterArena server-authoritative smoke
through `MultiplayerSandboxHeadless`. The smoke uses existing
`AuthoritativeMovementCore` to prove server-owned state, accepted input,
rejected invalid input, authoritative tick mutation, and snapshot report
evidence.

Accepted boundary:

- local headless server smoke only;
- one server-owned actor;
- one accepted fixed input;
- one rejected invalid input;
- one authoritative snapshot report;
- no full multiplayer gameplay, MMO-scale networking, Visual Blocks, editor
  workflow, package output, distribution output, or external client process.

## Changed Files

- `Tools/MultiplayerSandboxHeadless/include/MultiplayerSandboxHeadless/HeadlessPreview.hpp`
- `Tools/MultiplayerSandboxHeadless/src/HeadlessPreview.cpp`
- `Tools/MultiplayerSandboxHeadless/cli/main.cpp`
- `Tests/Unit/Samples/MultiplayerSandboxHeadlessTests.cpp`
- `samples/StarterArena/README.md`
- `samples/StarterArena/ACCEPTANCE.md`
- `samples/StarterArena/KNOWN_LIMITATIONS.md`
- `docs/internal/PHASE_STATUS_MATRIX.md`
- `docs/internal/phase-evidence/PHASE_12/EVIDENCE.md`
- `docs/internal/phase-evidence/PHASE_12/commands.log`
- `docs/internal/phase-evidence/PHASE_12/changed_files.txt`
- `docs/internal/phase-evidence/PHASE_12/known_limitations.md`
- `docs/internal/phase-evidence/PHASE_12/verification_result.json`

## Verification Commands

- `Tools/Forge/bin/forge nix build --target MultiplayerSandboxHeadless --build build/RelWithDebInfo-0.0.9 --jobs=1`
- `build/RelWithDebInfo-0.0.9/bin/MultiplayerSandboxHeadless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-server-smoke --report-out /tmp/starter_arena_server_smoke.json --diagnostics-out /tmp/starter_arena_server_diagnostics --ticks 1 --fixed-dt 1.0`
- `Tools/Forge/bin/forge nix build --target MultiplayerSandboxHeadlessTests --build build/RelWithDebInfo-0.0.9 --jobs=1`
- `build/RelWithDebInfo-0.0.9/MultiplayerSandboxHeadlessTests`
- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 12`

## Command Results

The focused headless tool build passed. The real StarterArena server smoke
exited `0` and wrote `/tmp/starter_arena_server_smoke.json` with:

- `status: Passed`;
- `projectId: starter-arena`;
- `serverAuthority: true`;
- `networkMode: HeadlessSmoke`;
- `inputAcceptedCount: 1`;
- `inputRejectedCount: 1`;
- `snapshotCount: 1`;
- authoritative initial and final state;
- invalid input diagnostics;
- explicit non-claims.

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
- [x] Server smoke command was manually checked.
- [x] Unsupported behavior is not hidden.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

The focused StarterArena server-authoritative smoke passes locally and writes
bounded report evidence. Maintainer verification, full multiplayer gameplay,
MMO-scale networking, Visual Blocks, editor workflow, package output, and
distribution output remain absent.
