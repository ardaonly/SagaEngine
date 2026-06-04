# Phase 32 Evidence

## Status

Implemented-Unverified

## Phase Scope

Real SDE CLI Package Input Closure

Phase 32 closes only the `missing sde executable` package preflight blocker by
using the existing standalone SDE bootstrap staging path:

```bash
nix-shell --run "python3 Tools/SystemDefinitionEngine/build.py --clean --jobs 1"
```

That command builds the real `sde-cli` CMake target from
`Tools/SystemDefinitionEngine/CMakeLists.txt`, whose output name is `sde`, and
stages the resulting executable at:

```txt
Tools/SystemDefinitionEngine/bin/sde
```

`scripts/package-linux-saga` now includes that staged real executable path in
the `sde CLI` preflight candidates and still requires a non-empty executable.

This phase does not create a fake `sde` binary, does not create a placeholder
wrapper script, does not modify SDE compiler semantics, does not absorb SDE into
the root engine build, does not create final Linux distribution output, and does
not claim package or distribution readiness.

## Changed Files

See `changed_files.txt`.

## Verification Commands

- `test -x Tools/SystemDefinitionEngine/bin/sde`
- `Tools/SystemDefinitionEngine/bin/sde --help`
- `scripts/package-linux-saga`
- `python3 -m json.tool build/reports/linux_package_preflight_report.json`
- focused JSON assertion that `sde` is no longer listed as missing while
  archive/checksum blockers remain
- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 32`

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

The real standalone SDE CLI is built by the existing SDE bootstrap path and
staged at `Tools/SystemDefinitionEngine/bin/sde`. Package preflight accepts that
real executable as an SDE input and no longer reports `sde CLI` as missing when
the staged executable is present. The report still has `verified: false` and
continues to block on final distribution layout, archive, and checksum.
Maintainer verification is still required, so the phase is not Verified.
