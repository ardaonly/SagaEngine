# Saga Linux Distributable Candidate Gate

> Status: Linux distribution evidence aggregator

This document records how existing local package, archive, checksum, and smoke
evidence can be aggregated. It is not a release gate and does not claim
readiness.

`scripts/verify-linux-saga-candidate` collects the existing Linux package,
archive, checksum, unpack smoke, and StarterArena distribution workflow reports
into one machine-readable evidence report:

The report output path is caller-controlled or tool-defaulted local evidence.
It is not architecture truth.

The gate does not rebuild or restage the package. It validates existing evidence
from:

- a staged Linux layout;
- an archive;
- a checksum;
- a package preflight report;
- a distribution smoke report.

## Evidence Meaning

A bounded Linux distribution evidence report means:

- `scripts/package-linux-saga` produced a `preflight-passed` report;
- the staged layout exists;
- `Saga.tar.zst` exists;
- `Saga.sha256` exists and verifies;
- the archive entries are rooted at `Saga/`;
- the schema-v2 file manifest matches the layout exactly;
- application, public-tool, exclusion, license, and notice sets are exact;
- package, smoke, and candidate reports share the same package/version/commit
  identity;
- distribution smoke status is `passed-with-limitations`;
- unpacked tool help checks pass;
- unpacked StarterArena `sagaproject validate`, `sagascript analyze`, and
  `Saga --workflow-smoke` pass;
- current Runtime and Editor limitations are consumed from the smoke report.

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
- historical verified status.

## Current Limitations

The candidate gate does not hardcode Runtime or Editor outcomes. It carries the
current smoke report limitations forward, requires editor executable presence,
and does not launch the GUI in headless validation.

The Product Shell workflow report is no-UI and report-only. It is not proof that
referenced developer-tree commands ran from the packaged distribution.
