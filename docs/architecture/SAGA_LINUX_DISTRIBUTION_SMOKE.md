# Saga Linux Distribution Smoke

> Status: Linux distribution smoke evidence contract

This document records a bounded archive/unpack smoke check for local evidence.
It is not full distribution verification or product readiness.

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
- checks that archive entries are rooted at `Saga/`, safe, and permitted by the
  package whitelist before extraction;
- deletes and recreates the requested temporary work directory;
- unpacks the archive into that temporary directory;
- validates required files, directories, and executable bits from the unpacked
  tree;
- rejects forbidden basenames, unknown files, extra applications/tools, test
  executables, and every unrecognized executable-bit file;
- runs limited help commands from unpacked binaries only;
- runs a limited StarterArena workflow smoke from unpacked distribution paths
  only;
- writes a machine-readable distribution smoke report.

Required unpacked checks include:

```txt
Saga/bin/Saga
Saga/bin/SagaEditor
Saga/bin/SagaRuntime
Saga/tools/sagaproject
Saga/tools/sagascript
Saga/tools/sagapack
Saga/BUILD_INFO.json
Saga/VERSION
Saga/VERIFY.txt
Saga/KNOWN_LIMITATIONS.md
Saga/samples/StarterArena
```

## Current Limitations

The current passing status is `passed-with-limitations`.

`sagascript`, and `sagapack` help commands pass from the unpacked distribution
tree.

The unpacked StarterArena workflow smoke currently passes these required
distribution-only commands:

```txt
Saga/tools/sagaproject validate
Saga/tools/sagascript analyze
Saga/bin/Saga --workflow-smoke
```

`SagaEditor` is required and executable-presence checked. Its Editor-owned
`--inspect-project` path now emits clean schema 2, but archive smoke still does
not launch the GUI and does not treat inspection as full Editor readiness.

`SagaRuntime --starter-arena-smoke` remains an optional bounded command. Its
current exit code and output are recorded rather than converted into a fixed
historical blocker statement.

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
- product dedicated-server workflow correctness;
- tool workflow correctness beyond recorded help checks;
- historical verified status.
