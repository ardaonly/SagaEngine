# SagaEngine Licensing

SagaEngine uses a path-based license model. The monorepo is the canonical
source of truth; external Prism, Forge, or SDE repositories are mirrors or
release exports only.

## Apache-2.0 Areas

The engine core, runtime, server, backends, tests, build system, and tooling are
licensed under Apache-2.0 unless a more specific path rule says otherwise.

Primary Apache areas:

- `Engine/`
- `Runtime/`
- `Server/`
- `Backends/`
- `Tools/Prism/`
- `Tools/Forge/tool/`
- `Tools/SystemDefinitionEngine/`
- `Tests/`
- `docs/`
- root build and configuration files

The Apache license text is stored at `LICENSES/LICENSE`. Standalone tool exports
also carry their own `LICENSE` file.

## Restricted Editor Areas

The following paths are source-available but not Apache licensed:

- `Editor/`
- `Apps/Editor/`
- `Apps/EditorLab/`

These paths are covered by `LICENSES/LICENSE-EDITOR`. They are read-only for
external users, commercial/private for distribution, and may not be used for AI
training, dataset generation, embedding corpora, or model input redistribution.

## Boundary Inventory

`core/manifest/path_rules.json` is the declarative inventory for path ownership
and license intent. In Phase 1 it is report-only: scripts may warn about drift,
but it is not a hard policy engine.
