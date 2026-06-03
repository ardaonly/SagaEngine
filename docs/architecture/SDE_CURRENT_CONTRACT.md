# SDE Current Contract

This document records the current System Definition Engine boundary. It is a
stabilization note, not a new feature specification.

## Current Boundary

SDE lives under `Tools/SystemDefinitionEngine/` as a standalone C++ package:

- public headers are under `include/SDE/**`;
- implementation is under `src/SDE/**`;
- the command-line entry point is `cli/main.cpp`;
- unit and integration tests are under `tests/**`;
- `CMakeLists.txt`, `cmake/SDEConfig.cmake.in`, and `conanfile.py` define the
  standalone CMake and Conan package boundary.

The public target is `SDE::Core`. The `SDE::SDE` alias remains only as a pre-1.0
compatibility alias. SDE depends on the C++ standard library and
`nlohmann_json`; tests additionally use GTest through the package/test flow.

Current subsystems in the SDE tree are:

- `Core`: versions, stable hashes, cancellation;
- `Model`: model, field, relation, type, and enum definitions;
- `Validation` and `Diagnostics`: rules, validation state, structured
  diagnostics;
- `IO`: JSON model loading and model writing;
- `Compilation`: reference resolution and compiled read-only graph production;
- `Compiler`: session/facade APIs, compile requests/results, dependency
  manifests;
- `Artifacts`: artifact manifest metadata and reading surfaces.

## Intended Role

SDE is a standalone definition compiler. Its current contract is to:

- load authored schema/data definitions;
- validate schema, data, rules, versions, and references;
- produce deterministic read-only compiled graph/artifact metadata;
- emit structured diagnostics with stable machine-readable codes;
- expose package-consumable CMake/Conan targets for downstream tools.

SDE is not:

- `SagaRuntime`;
- `SagaEditor`;
- `SagaServer`;
- a SagaScript or C# scripting host;
- a Visual Blocks engine;
- the project/package/distribution owner;
- a runtime gameplay loop or multiplayer authority layer.

SagaEngine-side tools may consume SDE through `SDE::Core` and public headers.
That direction does not make SDE depend on SagaEngine. SDE core must remain
extractable as an independent package.

## Boundary Rules

SDE core must not include or link engine/editor/server/runtime internals. In
particular, SDE source should not reference:

- `SagaEngine`, `SagaEditor`, `SagaServer`, or `SagaRuntime` headers;
- repo-local engine paths such as `Engine/`, `Editor/`, `Runtime/`, `Server/`,
  `Apps/`, or `Collaboration/`;
- Forge, Prism, or Saga product targets;
- SagaScript, CSharpScriptHost, Visual Blocks editor, or runtime graph
  execution implementation.

Saga-specific semantics belong in adapter/tool layers outside the SDE core
package. If a downstream Saga tool needs to compile Saga-specific data, it
should translate those inputs before or after calling SDE public APIs.

## Current Verification

The current boundary gate is:

```bash
scripts/check-boundaries
```

It scans the SDE tree for forbidden engine/editor/server/runtime references,
repo-local engine paths, Forge/Prism dependencies, and Saga target links.

The standalone package/test proof is:

```bash
nix-shell --run 'conan create Tools/SystemDefinitionEngine -o "&:build_tests=True" --build=missing -s build_type=Release -c tools.build:jobs=1'
```

This builds the SDE library and CLI as a Conan package, runs `SDETests`, and
installs the package headers, static library, CLI, license, and CMake config.
The `tools.build:jobs=1` setting is intentional for local stability.

## Known Limitations

`python3 Tools/SystemDefinitionEngine/build.py --tests` is the documented
bootstrap command, but in the current local environment it does not complete via
the script path. Outside `nix-shell`, Conan is unavailable on `PATH`. Inside
`nix-shell`, the script runs `conan install` but the subsequent CMake configure
does not resolve `nlohmann_json`.

The Conan package path above does build and test SDE standalone. The bootstrap
script limitation should not be hidden or treated as maintainer verification.
