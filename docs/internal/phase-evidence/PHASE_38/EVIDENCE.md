# Phase 38 Evidence

## Status

Implemented-Unverified

## Phase Scope

Distributable Technical Preview Candidate Gate

Phase 38 adds machine-readable candidate and contract gates for the Linux
technical-preview distribution evidence. The gates validate existing package,
archive, checksum, unpack smoke, BUILD_INFO contract fields, public `bin/` tool
entrypoints, staged public docs, and limited StarterArena distribution workflow
reports without claiming production readiness, enterprise readiness, verified
final release status, full distribution verification, full editor workflow, full
Visual Blocks UI, or full gameplay readiness.

## Changed Files

See `changed_files.txt`.

## Verification Commands

See `commands.log`.

## Command Results

- `scripts/package-linux-saga` exits `0` with a valid package preflight report.
- `scripts/smoke-linux-saga-dist` exits `0` with status `passed-with-limitations`.
- `scripts/verify-linux-saga-candidate` exits `0` with status
  `candidate-with-limitations`.
- `scripts/verify-linux-distribution-contract` exits `0` with status
  `passed-with-limitations`.
- `linux_distributable_candidate_report.json` validates as JSON and references
  the package report, smoke report, archive, checksum, and staged layout.
- `linux_distribution_contract_report.json` validates as JSON, keeps
  `verified: false`, and records final acceptance gaps.
- Runtime and Editor packaged workflow blockers remain recorded.

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
- [x] Package, smoke, candidate, and contract evidence are consumed from
  existing reports.
- [x] Runtime and Editor blockers remain visible.
- [x] Final-only SDK/library/share payload gaps remain visible.
- [x] Unsupported behavior is not hidden.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

Phase 38 has candidate and contract gate reports that validate the existing
Linux package, archive, checksum, unpack smoke, limited StarterArena workflow
evidence, public `bin/` tool entrypoints, and BUILD_INFO contract shape. Runtime
and Editor packaged workflow blockers remain recorded, final acceptance gaps
remain visible, and maintainer verification has not occurred.
