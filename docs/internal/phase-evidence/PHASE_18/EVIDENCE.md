# Phase 18 Evidence

## Status

Implemented-Unverified

## Phase Scope

SDE Current Contract Stabilization

This phase audits and documents the current System Definition Engine boundary.
It does not add new SDE features and does not modify runtime, server, editor,
SagaScript, Visual Blocks, package/distribution, StarterArena gameplay, or
CSharpScriptHost behavior.

## Evidence Summary

Phase 18 adds a current contract document:

- `docs/architecture/SDE_CURRENT_CONTRACT.md`

The document records that SDE is currently a standalone C++ definition compiler
and deterministic artifact/diagnostics producer under
`Tools/SystemDefinitionEngine/`. It also records explicit non-roles:

- not `SagaRuntime`;
- not `SagaEditor`;
- not `SagaServer`;
- not a SagaScript or C# scripting host;
- not a Visual Blocks engine;
- not the project/package/distribution owner.

The current SDE package boundary remains:

- public headers under `Tools/SystemDefinitionEngine/include/SDE/**`;
- implementation under `Tools/SystemDefinitionEngine/src/SDE/**`;
- CLI entry point at `Tools/SystemDefinitionEngine/cli/main.cpp`;
- tests under `Tools/SystemDefinitionEngine/tests/**`;
- standalone CMake/Conan package contract through
  `Tools/SystemDefinitionEngine/CMakeLists.txt`,
  `Tools/SystemDefinitionEngine/cmake/SDEConfig.cmake.in`, and
  `Tools/SystemDefinitionEngine/conanfile.py`;
- public CMake target `SDE::Core`.

## Boundary Checks

The existing boundary gate passed:

```txt
check-boundaries: scanned 93 SDE file(s)
check-boundaries: SDE standalone source-boundary scan passed
```

The gate scans the SDE tree for forbidden SagaEngine, SagaEditor, SagaServer,
SagaRuntime, repo-local engine path, Forge/Prism, and Saga target references.
The count includes ignored local SDE build files created while running the
standalone package/test proof.

## Standalone Package/Test Check

The standalone Conan package path passed with one build job:

```bash
nix-shell --run 'conan create Tools/SystemDefinitionEngine -o "&:build_tests=True" --build=missing -s build_type=Release -c tools.build:jobs=1'
```

Result:

```txt
SDETests: passed
100% tests passed, 0 tests failed out of 1
sde/0.1.1: Package created
```

The command built SDE from the package recipe, ran the SDE test binary, and
installed package headers, static library, CLI, license, and CMake config into
the Conan package folder.

## Bootstrap Script Limitation

The direct bootstrap test command did not complete in this environment:

```bash
python3 Tools/SystemDefinitionEngine/build.py --tests --jobs 1
nix-shell --run "python3 Tools/SystemDefinitionEngine/build.py --tests --jobs 1"
```

Outside `nix-shell`, `conan` was not on `PATH`. Inside `nix-shell`, Conan
install completed, but the subsequent CMake configure could not resolve
`nlohmann_json`. This is recorded as a bootstrap-path limitation, not hidden as
success. The separate Conan package/test path above passed.

## Changed Files

See `changed_files.txt`.

## Verification Commands

See `commands.log`.

## Command Results

The recorded local checks passed after the documentation/evidence update. The
phase remains `Implemented-Unverified` because maintainer verification has not
occurred.

## Required Files

- `EVIDENCE.md`: present
- `commands.log`: present
- `changed_files.txt`: present
- `known_limitations.md`: present
- `verification_result.json`: present

## Manual Checks

- [x] Public docs do not overclaim.
- [x] Known limitations are documented.
- [x] No placeholder is presented as shipped behavior.
- [x] SDE boundary tooling was checked.
- [x] Unsupported bootstrap behavior is not hidden.
- [x] No phase is marked `Verified`.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

SDE current contract documentation and evidence now record the standalone
boundary, non-roles, boundary scan result, and Conan package/test proof. The
direct bootstrap test path has a documented dependency-resolution limitation in
this local environment. Maintainer verification, new SDE features, runtime,
server, editor, SagaScript, Visual Blocks, package/distribution, StarterArena
gameplay, and CSharpScriptHost changes remain absent.
