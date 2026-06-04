# Saga Linux Distribution Smoke

Phase 35 status is `Implemented-Unverified`. Phase 36 status is
`Implemented-Unverified`. Phase 37 status is `Implemented-Unverified`.

`scripts/smoke-linux-saga-dist` verifies the Linux archive by unpacking
`build/dist/linux/Saga.tar.zst` into a clean temporary directory and running a
limited smoke from the unpacked `Saga/` tree.

This is archive/unpack smoke evidence only. It does not claim production
readiness, enterprise readiness, verified final release status, full
distribution verification, full editor workflow, full Visual Blocks UI, or
cloud collaboration.

## Inputs

The distribution input is only:

```txt
build/dist/linux/Saga.tar.zst
```

The checksum input is:

```txt
build/dist/linux/Saga.sha256
```

The script does not use `build/dist/linux/Saga/` as the smoke target and does
not silently fall back to repository tools or repository paths.

## Smoke Contract

The smoke script:

- verifies `Saga.sha256` with `sha256sum -c`;
- checks that the archive entries are rooted at `Saga/`;
- deletes and recreates the requested temporary work directory;
- unpacks the archive into that temporary directory;
- validates required files, directories, and executable bits from the unpacked
  tree;
- runs limited help commands from unpacked binaries only;
- runs a limited StarterArena workflow smoke from unpacked distribution paths
  only;
- writes `build/reports/linux_distribution_smoke_report.json`.

Required unpacked checks include:

```txt
Saga/bin/Saga
Saga/bin/SagaEditor
Saga/bin/SagaRuntime
Saga/bin/SagaServer
Saga/tools/sagaproject
Saga/tools/sagascript
Saga/tools/sagapack
Saga/tools/sde
Saga/BUILD_INFO.json
Saga/VERSION
Saga/VERIFY.txt
Saga/KNOWN_LIMITATIONS.md
Saga/samples/StarterArena
```

## Current Limitations

The current passing status is `passed-with-limitations`.

The unpacked `sde`, `Saga`, `SagaRuntime`, `SagaServer`, `sagaproject`,
`sagascript`, and `sagapack` help commands pass from the unpacked distribution
tree.

The unpacked StarterArena workflow smoke currently passes these required
distribution-only commands:

```txt
Saga/tools/sagaproject validate
Saga/tools/sagascript analyze
Saga/bin/Saga --workflow-smoke
```

`SagaEditor --help` is recorded as a skipped limitation because the binary does
not expose that help path.

`SagaRuntime --starter-arena-smoke` from the packaged distribution is recorded
as blocked because the current binary enters normal client startup, attempts UDP
transport setup, and does not produce the requested smoke report.

`SagaEditor --inspect-project` from the packaged distribution is recorded as
blocked because the current binary reports `unknown argument '--inspect-project'`.

The Product Shell workflow output is report-only. Any developer-tree command
references inside that product report are not executed by the distribution
smoke.

## Non-Claims

The smoke report does not claim:

- production readiness;
- enterprise readiness;
- verified final release status;
- full distribution verification;
- full editor workflow;
- full Visual Blocks UI;
- full gameplay readiness;
- cloud collaboration;
- runtime workflow correctness beyond help output;
- editor workflow correctness;
- server workflow correctness beyond help output;
- tool workflow correctness beyond recorded help checks;
- any phase `Verified` status.
