# Phase 35 Evidence

## Status

Implemented-Unverified

## Phase Scope

Distribution Unpack and Smoke Verification

Phase 35 adds `scripts/smoke-linux-saga-dist`, a separate unpack smoke for the
Linux archive:

```txt
build/dist/linux/Saga.tar.zst
```

The smoke verifies `Saga.sha256`, confirms the archive root is `Saga/`, unpacks
the archive into a clean temporary directory, checks required files and
executable bits from the unpacked `Saga/` tree, and runs limited help commands
from unpacked binaries only.

The smoke report is:

```txt
build/reports/linux_distribution_smoke_report.json
```

Current result is `passed-with-limitations`. Core unpacked help commands pass
for `sde`, `Saga`, `SagaRuntime`, and `SagaServer`. `SagaEditor --help` and the
unpacked .NET wrapper tools are recorded as limitations.

This phase does not claim production readiness, enterprise readiness, verified
final release status, full distribution verification, full editor workflow, full
Visual Blocks UI, cloud collaboration, or any phase `Verified` status.

## Changed Files

See `changed_files.txt`.

## Verification Commands

- `nix-shell --run "python3 Tools/SystemDefinitionEngine/build.py --clean --jobs 1"`
- `test -x Tools/SystemDefinitionEngine/bin/sde`
- `scripts/package-linux-saga`
- `python3 -m json.tool build/reports/linux_package_preflight_report.json`
- `cd build/dist/linux && sha256sum -c Saga.sha256`
- `tar -tf build/dist/linux/Saga.tar.zst`
- `scripts/smoke-linux-saga-dist --archive build/dist/linux/Saga.tar.zst --checksum build/dist/linux/Saga.sha256 --work-dir /tmp/saga_dist_smoke --report-out build/reports/linux_distribution_smoke_report.json`
- `python3 -m json.tool build/reports/linux_distribution_smoke_report.json`
- focused JSON assertion for required checks and non-claims
- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 35`

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

The smoke verifies checksum, archive root, unpack, required files, executable
bits, and limited unpacked help commands. It records current wrapper/editor
limitations honestly and keeps `verified: false`. Maintainer verification is
still required, so the phase is not Verified.
