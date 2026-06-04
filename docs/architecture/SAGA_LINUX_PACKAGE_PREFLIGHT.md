# Saga Linux Package Preflight

Phase 31 status is `Implemented-Unverified`. Phase 32 status is
`Implemented-Unverified`. Phase 33 status is `Implemented-Unverified`.

`scripts/package-linux-saga` is a Linux layout staging and package preflight
checker. It stages the first Linux layout from real existing inputs, then checks
for required final package outputs. It does not build binaries, create an
archive, create a checksum, or prove package or distribution readiness.

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
- `stagingStatus`
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
`status: "preflight-passed"`. A passed preflight still does not claim package
readiness, distribution readiness, or maintainer verification.

## Current Blockers





That path is valid only when produced by:







```txt
build/dist/linux/Saga
```

The layout is created only from real existing files and generated honest
metadata. It is not a final distribution.

After successful Phase 33 layout staging, the current preflight report remains
blocked by:

- missing `build/dist/linux/Saga.tar.zst`;
- missing `build/dist/linux/Saga.sha256`.



## Non-Claims

The Linux package preflight report does not claim:

- package readiness;
- distribution readiness;
- final archive readiness;
- checksum readiness;
- maintainer verification;
- runtime workflow correctness;
- editor workflow correctness;
- server workflow correctness;
- tool workflow correctness beyond input existence checks.

No phase is marked `Verified` by this report.

## Risks

