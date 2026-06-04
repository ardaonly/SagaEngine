# Saga Linux Distribution Layout

Phase 33 status is `Implemented-Unverified`. Phase 34 status is
`Implemented-Unverified`.

`scripts/package-linux-saga` stages the first Linux layout under:

```txt
build/dist/linux/Saga
```

The layout is not a final distribution. It is a staging directory assembled from
real existing binaries, tools, docs, samples, licenses, and generated metadata.
Phase 34 creates sibling archive/checksum outputs from this layout, but those
outputs do not prove production readiness, enterprise readiness, full
distribution validation, verified release status, or verified final release
status.

## Layout Contract

The staged layout is:

```txt
build/dist/linux/Saga/
  bin/
    Saga
    SagaEditor
    SagaRuntime
    SagaServer
  tools/
    sagaproject
    sagapack
    sagascript
    sde
  samples/
    StarterArena/
  docs/
    README.md
    product/
      GETTING_STARTED.md
      CURRENT_CAPABILITIES.md
      WHAT_IS_NOT_IMPLEMENTED.md
      CURRENT_DISTRIBUTION_STATUS.md
  licenses/
  VERSION
  VERIFY.txt
  KNOWN_LIMITATIONS.md
  BUILD_INFO.json
```

When `--without-server` is used, `SagaServer` is not required or staged.

## Real Inputs

Executables are copied only after they pass non-empty executable validation.
Phase 33 does not create fake binaries, wrapper scripts, or placeholder tools.

The SDE input is the real staged executable from:

```txt
Tools/SystemDefinitionEngine/bin/sde
```

That file is valid only when produced by the standalone SDE bootstrap build path:

```bash
nix-shell --run "python3 Tools/SystemDefinitionEngine/build.py --clean --jobs 1"
```

## Metadata

`VERSION` is copied from the repository root `VERSION` file.

`VERIFY.txt` contains staged-layout presence checks and archive/checksum
commands. It does not verify runtime, editor, server, tool workflows,
production readiness, enterprise readiness, full distribution validation, or
verified release status, or verified final release status.

`KNOWN_LIMITATIONS.md` states that the layout is not production-ready,
enterprise-ready, a verified final release, or full distribution validation. It
also states that the layout is not a verified release. It records current
missing product areas, including full editor UI, Visual Blocks editor UI,
enterprise collaboration, and cloud workspace.

`BUILD_INFO.json` records staged inputs, the build directory, output directory,
source commit when available, non-claims, and `verified: false`. It intentionally
omits nondeterministic timestamps.

## Archive And Checksum

Phase 34 creates these sibling files from the staged layout:

```txt
build/dist/linux/Saga.tar.zst
build/dist/linux/Saga.sha256
```

`Saga.tar.zst` is generated only from `build/dist/linux/Saga/` and contains
`Saga/` as its top-level directory. `Saga.sha256` covers only `Saga.tar.zst` and
is compatible with `sha256sum -c`.

The archive and checksum are evidence artifacts only. They do not claim
production readiness, enterprise readiness, full distribution validation, or
verified release status or verified final release status.

## Non-Claims

The staged layout does not claim:

- package readiness;
- distribution readiness;
- production readiness;
- enterprise readiness;
- verified release status;
- verified final release status;
- full distribution validation;
- maintainer verification;
- runtime workflow correctness;
- editor workflow correctness;
- server workflow correctness;
- tool workflow correctness beyond input existence checks;
- any phase `Verified` status.
