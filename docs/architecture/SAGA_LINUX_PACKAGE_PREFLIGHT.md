# Saga Linux Package Preflight

> Status: Linux package preflight evidence contract

This document records a local package preflight report contract. It is not a
release pipeline, package readiness claim, or final distribution validation.

`scripts/package-linux-saga` is a Linux layout staging and package preflight
checker. It stages the first Linux layout from real existing inputs, creates
`Saga.tar.zst` and `Saga.sha256` from that layout, then checks for required
package outputs. It does not build binaries, prove production readiness, prove
enterprise readiness, prove full distribution validation, or mark a verified
release or verified final release.

The script checks for existing real inputs and the expected final distribution
outputs, then writes a machine-readable report. The report path is local
evidence and can be redirected:

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
- `stagingStatus`
- `archiveGenerationStatus`
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
`status: "preflight-passed"`. A passed preflight still does not claim production
readiness, enterprise readiness, full distribution validation, verified final
release status, verified release status, or maintainer verification.

## Current Package Outputs

This document stages the first Linux layout at:

```txt
build/dist/linux/Saga
```

The layout is created only from real existing files and generated honest
metadata. It is not a final distribution.

After successful this document archive/checksum generation, the package preflight
report no longer lists the archive and checksum blockers when these real files
exist:

```txt
build/dist/linux/Saga.tar.zst
build/dist/linux/Saga.sha256
```

`Saga.tar.zst` is generated only from `build/dist/linux/Saga/` and contains
`Saga/` as the top-level archive directory. `Saga.sha256` covers only
`Saga.tar.zst` and is compatible with `sha256sum -c`.

## Non-Claims

The Linux package preflight report does not claim:

- production readiness;
- enterprise readiness;
- verified release status;
- verified final release status;
- full distribution validation;
- full editor UI readiness;
- Visual Blocks editor UI readiness;
- cloud collaboration readiness;
- maintainer verification;
- runtime workflow correctness;
- editor workflow correctness;
- server workflow correctness;
- tool workflow correctness beyond input existence checks.

No release or historical verified status is marked by this report.

## Risks

- The preflight report could be mistaken for package output.
- The staged layout could be mistaken for a final distribution.
- The archive could be mistaken for a production release.
- The checksum could be mistaken for full distribution validation.
- Placeholder binaries could hide missing tools.
- Archive or checksum language could imply distribution readiness.
- A future script success could mask missing runtime, editor, server, or tool
  workflows.
