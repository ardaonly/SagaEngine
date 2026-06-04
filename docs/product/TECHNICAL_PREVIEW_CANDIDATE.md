# Technical Preview Candidate Status

SagaEngine currently supports a Linux technical-preview candidate with
limitations.

This is not a public product release. Phase 1-38 are
`Implemented-Unverified`, and no phase is marked `Verified`.

## Current Status

Saga can produce a Linux technical-preview candidate archive with checksum and
limited distribution smoke evidence.

The current candidate status is based on generated local evidence:

- package report status: `preflight-passed`;
- distribution smoke report status: `passed-with-limitations`;
- sample executable evidence report status: `passed-with-limitations` when
  `SagaSampleExecutableEvidence` completes in the active build tree;
- candidate report status: `candidate-with-limitations`.

## What Exists

The Linux package/candidate pipeline produces or validates these outputs:

- staged Linux layout: `build/dist/linux/Saga/`;
- archive: `build/dist/linux/Saga.tar.zst`;
- checksum: `build/dist/linux/Saga.sha256`;
- package preflight report:
  `build/reports/linux_package_preflight_report.json`;
- distribution smoke report:
  `build/reports/linux_distribution_smoke_report.json`;
- candidate report:
  `build/reports/linux_distributable_candidate_report.json`;
- sample executable evidence report:
  `build/RelWithDebInfo-0.0.9/reports/sample_executable_evidence_report.json`;
- unpack smoke from `Saga.tar.zst`;
- packaged `sagaproject`, `sagascript`, `sagapack`, and `sde` tools;
- limited packaged StarterArena workflow smoke.

## Build And Package

Run from the repository root:

```bash
nix-shell --run "python3 Tools/SystemDefinitionEngine/build.py --clean --jobs 1"
scripts/package-linux-saga
```

`scripts/package-linux-saga` stages the Linux layout and creates the archive and
checksum from real existing inputs.

## Archive And Checksum Verification

```bash
cd build/dist/linux
sha256sum -c Saga.sha256
tar -tf Saga.tar.zst | head
```

The archive should contain `Saga/` as the top-level directory.

## Distribution Smoke

```bash
scripts/smoke-linux-saga-dist \
  --archive build/dist/linux/Saga.tar.zst \
  --checksum build/dist/linux/Saga.sha256 \
  --work-dir /tmp/saga_dist_smoke \
  --report-out build/reports/linux_distribution_smoke_report.json
```

The smoke unpacks the archive into `/tmp/saga_dist_smoke/Saga` and uses only
the unpacked distribution tree.

## Candidate Gate

```bash
scripts/verify-linux-saga-candidate \
  --package-report build/reports/linux_package_preflight_report.json \
  --smoke-report build/reports/linux_distribution_smoke_report.json \
  --archive build/dist/linux/Saga.tar.zst \
  --checksum build/dist/linux/Saga.sha256 \
  --layout build/dist/linux/Saga \
  --report-out build/reports/linux_distributable_candidate_report.json
```

The candidate gate collects the package, archive, checksum, unpack smoke, and
limited StarterArena distribution workflow evidence into one report.

## What Is Smoke-Checked

The current smoke evidence covers:

- archive checksum verification;
- archive unpack;
- required file and executable presence;
- packaged CLI help checks;
- packaged `sagaproject validate`;
- packaged `sagascript analyze`;
- packaged `Saga --workflow-smoke`.
- build-local sample executable evidence for bounded StarterArena runtime script
  invocation and MultiplayerSandbox fixture/projection/headless smoke.

## Known Limitations

- Runtime packaged StarterArena smoke remains blocked.
- Editor packaged inspect mode remains blocked.
- SagaEditor help and inspect limitations remain.
- Product Shell workflow output is no-UI and report-only.
- StarterArena sample executable evidence remains bounded smoke evidence.
- MultiplayerSandbox sample executable evidence remains fixture/projection/
  headless evidence, not playable multiplayer game evidence.
- Full editor UI workflow does not exist.
- Full Visual Blocks UI does not exist.
- Enterprise collaboration and cloud workspace do not exist.
- This is not production-ready.
- This is not enterprise-ready.
- This is not a verified final release.

## Non-Claims

The technical-preview candidate is:

- not production-ready;
- not enterprise-ready;
- not a verified final release;
- not full distribution certification;
- not full editor workflow;
- not full Visual Blocks UI;
- not full gameplay or runtime validation;
- not full collaboration or cloud workspace.
