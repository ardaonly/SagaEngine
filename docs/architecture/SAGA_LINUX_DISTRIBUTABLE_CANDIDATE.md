# Saga Linux Distributable Candidate Gate

Phase 38 status is `Implemented-Unverified`.

`scripts/verify-linux-saga-candidate` collects the existing Linux package,
archive, checksum, unpack smoke, and StarterArena distribution workflow reports
into one machine-readable candidate report:

```txt
build/reports/linux_distributable_candidate_report.json
```

The gate does not rebuild or restage the package. It validates existing evidence
from:

```txt
build/dist/linux/Saga/
build/dist/linux/Saga.tar.zst
build/dist/linux/Saga.sha256
build/reports/linux_package_preflight_report.json
build/reports/linux_distribution_smoke_report.json
```

## Candidate Meaning

`candidate-with-limitations` means:

- `scripts/package-linux-saga` produced a `preflight-passed` report;
- the staged layout exists;
- `Saga.tar.zst` exists;
- `Saga.sha256` exists and verifies;
- the archive entries are rooted at `Saga/`;
- distribution smoke status is `passed-with-limitations`;
- unpacked tool help checks pass;
- unpacked StarterArena `sagaproject validate`, `sagascript analyze`, and
  `Saga --workflow-smoke` pass;
- Runtime and Editor workflow blockers are still recorded.

## Non-Claims

The candidate report does not claim:

- production readiness;
- enterprise readiness;
- verified final release status;
- full distribution verification or certification;
- full editor workflow;
- full Visual Blocks UI;
- full gameplay readiness;
- full runtime validation;
- cloud collaboration;
- any phase `Verified` status.

## Current Blockers

The packaged Runtime StarterArena smoke remains blocked because the packaged
`SagaRuntime` path enters normal client startup, attempts UDP transport setup,
and does not write the requested smoke report.

The packaged Editor inspect command remains blocked because packaged
`SagaEditor` reports `unknown argument '--inspect-project'`.

The Product Shell workflow report is no-UI and report-only. It is not proof that
referenced developer-tree commands ran from the packaged distribution.
