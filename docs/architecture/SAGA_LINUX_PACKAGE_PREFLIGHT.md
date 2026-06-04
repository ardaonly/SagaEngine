# Saga Linux Package Preflight

Phase 31 status is `Implemented-Unverified`. Phase 32 status is
`Implemented-Unverified`.

`scripts/package-linux-saga` is a Linux package preflight checker. It does not
build binaries, stage a distribution tree, create an archive, create a checksum,
or prove package or distribution readiness.

The script checks for existing real inputs and the expected final distribution
outputs, then writes a machine-readable report. The default report path is:

```bash
build/reports/linux_package_preflight_report.json
```

The report can be redirected:

```bash
scripts/package-linux-saga --report-out /tmp/linux_package_preflight_report.json
```

## Report Contract

The report schema records:

- `schemaVersion`
- `tool: "package-linux-saga"`
- `status: "blocked"` or `"preflight-passed"`
- `verified: false`
- `config`
- `buildDir`
- `expectedOutput`
- `requiredInputs`
- `availableInputs`
- `missingInputs`
- `distributionLayoutStatus`
- `archiveStatus`
- `checksumStatus`
- `diagnostics`
- `knownLimitations`
- `nonClaims`

`requiredInputs` contains every checked item. Each checked item is also recorded
in either `availableInputs` or `missingInputs`.

If any checked item is missing, the script exits `1`, sets
`status: "blocked"`, and records the exact blocker strings in `diagnostics`.
If every checked item is available, the script exits `0` and sets
`status: "preflight-passed"`. A passed preflight still does not mean the script
has staged distribution output.

## Current Blockers

After Phase 32, `scripts/package-linux-saga` also checks the real SDE
bootstrap staging path:

```txt
Tools/SystemDefinitionEngine/bin/sde
```

That path is valid only when produced by:

```bash
nix-shell --run "python3 Tools/SystemDefinitionEngine/build.py --clean --jobs 1"
```

Phase 32 does not create a fake `sde` binary, does not add a wrapper script, and
does not absorb SDE into the root engine build. `scripts/package-linux-saga`
still validates the candidate as a non-empty executable.

With the real staged SDE CLI present, the current preflight report remains
blocked by:

- missing final distribution layout at `build/dist/linux/Saga`;
- missing `build/dist/linux/Saga/VERSION`;
- missing `build/dist/linux/Saga/VERIFY.txt`;
- missing `build/dist/linux/Saga/KNOWN_LIMITATIONS.md`;
- missing `build/dist/linux/Saga.tar.zst`;
- missing `build/dist/linux/Saga.sha256`.

The standalone SDE source contains a real `sde-cli` target that outputs `sde`.
Phase 32 uses the existing standalone SDE bootstrap staging path as package
preflight input evidence. It does not build a Linux distribution package and
does not stage final distribution output.

## Non-Claims

The Linux package preflight report does not claim:

- package readiness;
- distribution readiness;
- maintainer verification;
- runtime workflow correctness;
- editor workflow correctness;
- server workflow correctness;
- tool workflow correctness beyond input existence checks.

No phase is marked `Verified` by this report.

## Risks

- The preflight report could be mistaken for package output.
- The real staged SDE CLI could be mistaken for full package readiness.
- SDE package proof could be mistaken for a staged `sde` executable.
- Placeholder binaries could hide missing tools.
- Archive or checksum language could imply distribution readiness.
- A future script success could mask missing runtime, editor, server, or tool
  workflows.
