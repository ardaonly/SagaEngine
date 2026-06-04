# Phase 28 Evidence

## Status

Implemented-Unverified

## Phase Scope

Permissions / Roles v1

Phase 28 adds a no-UI Product Shell local role and permission intent metadata
report:

```bash
build/RelWithDebInfo-0.0.9-sde/bin/Saga --local-workspace-role-smoke --project samples/StarterArena/StarterArena.sagaproj --workspace builtin:basic --actor local.actor --role local.reviewer --permission inspect_project --role-report-out /tmp/starter_arena_role_permission_report.json
```

The report records one report-only role assignment and one represented
permission intent over the StarterArena project manifest. The report includes
`verified: false`, writes no durable role or permission service state, enforces
no policy, and does not mutate project truth.

The proof does not implement permission enforcement, secure access control,
enterprise policy, durable role service, durable permission service, networked
authorization, full multiplayer collaboration, cloud workspace, real-time team
editing, CRDT/OT, a collaboration server, full team workspace, product beta,
package readiness, or distribution readiness.

## Changed Files

See `changed_files.txt`.

## Verification Commands

- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 28`
- `nix-shell --run "cmake --build build/RelWithDebInfo-0.0.9-sde --target Saga SagaProductTests --parallel 1"`
- `build/RelWithDebInfo-0.0.9-sde/bin/SagaProductTests --gtest_filter='*Role*:*Permission*:*Slice*:*Visibility*:*LocalWorkspace*:*StarterArena*'`
- `build/RelWithDebInfo-0.0.9-sde/bin/Saga --local-workspace-role-smoke --project samples/StarterArena/StarterArena.sagaproj --workspace builtin:basic --actor local.actor --role local.reviewer --permission inspect_project --role-report-out /tmp/starter_arena_role_permission_report.json`

## Command Results

The listed commands are the required verification set for this phase. Record
final command results in `commands.log`.

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
- [x] Runtime/editor/tool behavior was manually checked if required.
- [x] Unsupported behavior is not hidden.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

Saga now has a no-UI local actor role and permission intent metadata report over
StarterArena. It documents report-only, non-durable, non-enforced behavior and
records explicit collaboration, cloud, enterprise, server, product beta, and
distribution non-claims. Maintainer verification is still required, so the
phase is not Verified.
