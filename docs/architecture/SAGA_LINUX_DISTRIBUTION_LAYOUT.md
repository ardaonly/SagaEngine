# Saga Linux Distribution Layout

> Status: Linux distribution evidence contract

This document records the expected staged Linux layout for local evidence. It
is not a product release, package readiness claim, or current onboarding path.

`scripts/package-linux-saga` stages the first Linux layout under:

```txt
build/dist/linux/Saga
```

The layout is not a final distribution. It is a staging directory assembled from
real existing binaries, tools, docs, samples, licenses, and generated metadata.
This document creates sibling archive/checksum outputs from this layout, but those
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
    .saga-tools/
      sagaproject/
      sagapack/
      sagascript/
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

Executables are copied or generated only after their real backing artifacts pass
validation. This document did not create fake binaries, wrapper scripts, or
placeholder tools. This document adds distribution-mode wrappers for `sagaproject`,
`sagascript`, and `sagapack`; those wrappers dispatch only to real published
.NET tool artifacts staged under `tools/.saga-tools/` and do not fall back to
repository paths.

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

## Packaged Tools

This document publishes the real `net10.0` CLI projects for `sagaproject`,
`sagascript`, and `sagapack` with `dotnet publish --self-contained false`.
The staged `tools/<name>` files are generated launchers that execute the
packaged DLLs from `tools/.saga-tools/<name>/` through the discovered .NET 10
command. They are not placeholder wrappers and do not use repository `.csproj`
files.

The packaged tool proof is limited to unpacked distribution help checks. It
does not claim full tool workflows, production readiness, enterprise readiness,
full distribution validation, verified release status, or verified final release
status.

## Archive And Checksum

This document creates these sibling files from the staged layout:

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
- historical verified status.
