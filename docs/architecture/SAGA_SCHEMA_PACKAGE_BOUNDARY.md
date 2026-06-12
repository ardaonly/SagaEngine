# Saga Schema Package Boundary

This document defines the current boundary between standalone SDE artifacts and
future Saga-specific schema/package consumption. It records current reality; it
does not create a schema package, package output, distribution output, runtime
integration, editor workflow, or Visual Blocks implementation.

## Current Reality

There is no dedicated Saga schema package tree in this repository today. In
particular, this document does not create:

- `share/saga/schemas/`;
- fake schema package folders;
- `Tools/SystemDefinitionEngine/examples/saga/`.

Current SDE truth is documented in:

- `docs/architecture/SDE_CURRENT_CONTRACT.md`;
- `docs/architecture/SDE_ARTIFACT_MANIFEST_CONTRACT.md`.

SDE Core is a standalone, engine-neutral definition compiler. It emits generic
artifacts such as `manifest.json`, `diagnostics.json`, `hashes.json`, and
`graph.json`. Those outputs are deterministic compiler artifacts, not SagaEngine
package/distribution artifacts.

Current Saga-facing surfaces are adapter or consumer surfaces outside SDE Core:

- CMake can wire Saga targets to `SDE::Core` when `SAGA_WITH_SDE=ON`;
- `SagaEditorCompositionLib` is documented in CMake as Saga-specific artifact
  generation outside the standalone SDE package tree;
- `SagaEditorLib` and `SagaProductLib` may link `SDE::Core` when SDE is enabled;
- `scripts/package-linux-saga` checks for an `sde` CLI as a required input but
  remains a preflight-only script;
- samples contain Saga project and package metadata, but that metadata is not a
  standalone Saga schema package.

## Dependency Direction

The dependency direction is one-way:

```txt
SDE Core
  -> generic definition artifacts and manifests
  -> consumed by Saga adapters/tools when enabled
```

Saga consumers may depend on SDE outputs and public `SDE::Core` APIs. SDE Core
must not depend on Saga consumers.

SDE Core must not depend on:

- SagaRuntime;
- SagaEditor;
- SagaServer;
- SagaScript or CSharpScriptHost;
- Visual Blocks editor/projection/runtime graph implementation;
- StarterArena gameplay;
- Saga package/distribution internals.

## Boundary Model

The intended model is:

```txt
SDE Core
  Generic definition compiler
  Generic artifact/manifest/diagnostic output

Saga Schema Package
  Future Saga-owned schema/adaptor metadata
  Future templates and validators
  Future mapping from generic SDE artifacts to Saga workflows

Saga Consumers
  Runtime/editor/product/package tools
  Read Saga-owned package artifacts when those artifacts exist
```

Saga-specific logic belongs in the Saga schema package or consumer adapters, not
inside SDE Core.

## Future Package Shape

If a future milestone adds a real Saga schema package, it should be outside the SDE
Core source tree and should be created only when a real consumer requires it.
A possible future shape is:

```txt
<saga-schema-package>/
  manifest.json
  schemas/
  adapters/
  templates/
  validators/
```

This is a future package shape, not current implementation. This document does not
claim package readiness or distribution readiness.

## Verification Boundary

This document evidence is limited to:

- documenting the current absence of a dedicated Saga schema package tree;
- documenting the correct dependency direction;
- keeping SDE Core engine-neutral;
- passing the SDE boundary scan and local milestone gates.

Distribution readiness, package staging, runtime/editor consumption, and final
schema package layout remain future work.
