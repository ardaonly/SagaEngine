# Phase 36 Evidence

## Status

Implemented-Unverified

## Phase Scope

Packaged Tool Wrapper Closure

Phase 36 closes the unpacked Linux distribution blocker where `sagaproject`,
`sagascript`, and `sagapack` were executable but failed because their copied
developer-tree wrappers expected adjacent `.csproj` files.

`scripts/package-linux-saga` now stages real framework-dependent `dotnet
publish` outputs for the three tool projects under `Saga/tools/.saga-tools/`.
The visible `Saga/tools/<name>` launchers execute those packaged artifacts and
do not fall back to repository paths.

## Changed Files

See `changed_files.txt`.

## Verification Commands

See `commands.log`.

## Command Results

- `scripts/package-linux-saga` exits `0` and regenerates `build/dist/linux/Saga.tar.zst` plus `build/dist/linux/Saga.sha256`.
- `scripts/smoke-linux-saga-dist` exits `0` with status `passed-with-limitations`.
- Unpacked `sagaproject --help`, `sagascript --help`, and `sagapack --help` pass from `/tmp/saga_dist_smoke/Saga/tools/`.
- `SagaEditor --help` remains a skipped limitation.

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
- [x] Packaged tool help commands were checked from the unpacked archive tree.
- [x] Unsupported behavior is not hidden.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

Phase 36 has real package and unpack smoke evidence for the three packaged tool
help commands, but maintainer verification has not occurred and no phase is
marked `Verified`.
