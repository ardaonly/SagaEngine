# Saga Linux Distribution Layout

Phase 33 status is `Implemented-Unverified`.

`scripts/package-linux-saga` stages the first Linux layout under:

```txt
build/dist/linux/Saga
```

The layout is not a final distribution. It is a staging directory assembled from
real existing binaries, tools, docs, samples, licenses, and generated metadata.
The script does not create `Saga.tar.zst` or `Saga.sha256`.

## Layout Contract

The staged layout is:



When `--without-server` is used, `SagaServer` is not required or staged.

## Real Inputs

Executables are copied only after they pass non-empty executable validation.
Phase 33 does not create fake binaries, wrapper scripts, or placeholder tools.









## Metadata

`VERSION` is copied from the repository root `VERSION` file.

`VERIFY.txt` contains staged-layout presence checks only. It does not verify
runtime, editor, server, tool workflows, archive integrity, checksum integrity,
package readiness, or distribution readiness.

`KNOWN_LIMITATIONS.md` states that the layout is not production-ready,
package-ready, or distribution-ready. It also records the current missing
product areas, including full editor UI, Visual Blocks editor UI, enterprise
collaboration, and cloud workspace.

`BUILD_INFO.json` records staged inputs, the build directory, output directory,
source commit when available, non-claims, and `verified: false`. It intentionally
omits nondeterministic timestamps.

## Remaining Blockers

After successful layout staging, package preflight is still expected to exit
`1` until these real outputs exist:

```txt
build/dist/linux/Saga.tar.zst
build/dist/linux/Saga.sha256
```

Phase 33 does not create those files and does not claim package or distribution
readiness.

## Non-Claims

The staged layout does not claim:

- package readiness;
- distribution readiness;
- production readiness;
- archive readiness;
- checksum readiness;
- maintainer verification;
- runtime workflow correctness;
- editor workflow correctness;
- server workflow correctness;
- tool workflow correctness beyond input existence checks;
- any phase `Verified` status.
