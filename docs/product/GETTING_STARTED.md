# Getting Started

Use this path if you are new to SagaEngine.

## Read First

1. [What Is SagaEngine?](WHAT_IS_SAGAENGINE.md)
2. [Saga Ecosystem Map](SAGA_ECOSYSTEM_MAP.md)
3. [Current Capabilities](CURRENT_CAPABILITIES.md)
4. [What Is Not Implemented](WHAT_IS_NOT_IMPLEMENTED.md)
5. [Current Distribution Status](CURRENT_DISTRIBUTION_STATUS.md)

## First Local Checks

Start with repo state and formatting:

```sh
git status --short
git diff --check
```

If a build tree already exists, use focused CMake/CTest commands for the
subsystem you are reviewing. For NixOS environments, run commands through
`nix-shell --run`.

## What To Avoid

- Do not assume the editor is product-ready.
- Do not assume `MultiplayerSandbox` is a playable game.
- Do not assume all tests or heavy stress suites pass.
- Do not treat generated reports as source files unless a fixture document says
  so.
- Do not use old alpha, technical-preview, candidate, tutorial, or planning
  notes as current onboarding material.

## Where To Go Next

- [Architecture index](../architecture/README.md)
- [Testing index](../testing/README.md)
- [`.sagaproj` Schema v0](SAGAPROJ_SCHEMA_V0.md)
