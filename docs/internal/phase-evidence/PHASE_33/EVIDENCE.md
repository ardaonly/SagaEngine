# Phase 33 Evidence

## Status

Implemented-Unverified

## Phase Scope

Linux Distribution Layout Staging

Phase 33 creates the first honest Linux layout staging path:

```txt
build/dist/linux/Saga
```

The layout is staged by `scripts/package-linux-saga` from real existing inputs:
Saga role binaries, wrapper CLIs, the real staged SDE CLI, StarterArena,
product docs, `README.md`, `LICENSES`, and generated metadata.

This phase does not create fake binaries, wrapper scripts, `Saga.tar.zst`, or
`Saga.sha256`. It does not claim package readiness or distribution readiness.

## Changed Files

See `changed_files.txt`.

## Verification Commands

- `nix-shell --run "python3 Tools/SystemDefinitionEngine/build.py --clean --jobs 1"`
- `test -x Tools/SystemDefinitionEngine/bin/sde`
- `scripts/package-linux-saga`
- `python3 -m json.tool build/reports/linux_package_preflight_report.json`
- focused JSON assertion that the staged layout and metadata exist while
  archive/checksum blockers remain
- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 33`

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

The package script stages `build/dist/linux/Saga` from real existing files and
generated honest metadata. The machine-readable report keeps `verified: false`
and continues to block on `Saga.tar.zst` and `Saga.sha256`. Maintainer
verification is still required, so the phase is not Verified.
