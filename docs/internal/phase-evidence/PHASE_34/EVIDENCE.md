# Phase 34 Evidence

## Status

Implemented-Unverified

## Phase Scope

Linux Archive and Checksum Generation

Phase 34 creates the first honest Linux archive and checksum from the staged
layout:

```txt
build/dist/linux/Saga
```

The generated outputs are:

```txt
build/dist/linux/Saga.tar.zst
build/dist/linux/Saga.sha256
```

`Saga.tar.zst` is generated only from the staged layout and contains `Saga/` as
the top-level directory. `Saga.sha256` covers only `Saga.tar.zst` and is
compatible with `sha256sum -c`.

This phase does not create fake archive/checksum outputs, does not claim
production readiness, does not claim enterprise readiness, does not claim a
verified release, does not claim a verified final release, and does not claim
full distribution validation.

## Changed Files

See `changed_files.txt`.

## Verification Commands

- `nix-shell --run "python3 Tools/SystemDefinitionEngine/build.py --clean --jobs 1"`
- `test -x Tools/SystemDefinitionEngine/bin/sde`
- `scripts/package-linux-saga`
- `python3 -m json.tool build/reports/linux_package_preflight_report.json`
- `test -d build/dist/linux/Saga`
- `test -f build/dist/linux/Saga.tar.zst`
- `test -f build/dist/linux/Saga.sha256`
- `cd build/dist/linux && sha256sum -c Saga.sha256`
- `tar -tf build/dist/linux/Saga.tar.zst`
- focused JSON assertion that archive/checksum blockers are gone while
  readiness remains non-claimed
- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 34`

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

The package script creates `Saga.tar.zst` from the staged Linux layout and
creates `Saga.sha256` from that archive. The checksum verifies with
`sha256sum -c`, the archive lists with `Saga/` as the root, and the report keeps
`verified: false` with production, enterprise, verified release, verified final
release, and full distribution validation non-claims. Maintainer verification is
still required, so the phase is not Verified.
