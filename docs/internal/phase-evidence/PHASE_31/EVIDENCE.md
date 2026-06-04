# Phase 31 Evidence

## Status

Implemented-Unverified

## Phase Scope

Package Pipeline v1

Phase 31 adds a machine-readable Linux package preflight report to
`scripts/package-linux-saga`.

The script remains preflight-only. It does not create package files, stage the
final distribution layout, build the SDE CLI, create an archive, create a
checksum, or claim package/distribution readiness.

The default report path is:

```bash
build/reports/linux_package_preflight_report.json
```

The current report is `status: "blocked"` and `verified: false`.

## Changed Files

See `changed_files.txt`.

## Verification Commands

- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 31`
- `scripts/package-linux-saga`
- `python3 -m json.tool build/reports/linux_package_preflight_report.json`
- `scripts/package-linux-saga --without-server`
- `scripts/package-linux-saga --report-out /tmp/linux_package_preflight_report.json`
- `python3 -m json.tool /tmp/linux_package_preflight_report.json`

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

`scripts/package-linux-saga` now writes an honest JSON preflight report with
`verified: false`, exact current blockers, available/missing input
classification, distribution layout status, archive status, checksum status,
known limitations, and non-claims. The script still exits `1` while real inputs
or final distribution outputs are missing. Maintainer verification is still
required, so the phase is not Verified.
